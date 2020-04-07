/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReportingHeader.h"

#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject
#include "js/JSON.h"
#include "mozilla/dom/ReportingBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsIEffectiveTLDService.h"
#include "nsIHttpChannel.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIRandomGenerator.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsXULAppAPI.h"

#define REPORTING_PURGE_ALL "reporting:purge-all"
#define REPORTING_PURGE_HOST "reporting:purge-host"

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<ReportingHeader> gReporting;

}  // namespace

/* static */
void ReportingHeader::Initialize() {
  MOZ_ASSERT(!gReporting);
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return;
  }

  RefPtr<ReportingHeader> service = new ReportingHeader();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->AddObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC, false);
  obs->AddObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  obs->AddObserver(service, "clear-origin-attributes-data", false);
  obs->AddObserver(service, REPORTING_PURGE_HOST, false);
  obs->AddObserver(service, REPORTING_PURGE_ALL, false);

  gReporting = service;
}

/* static */
void ReportingHeader::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gReporting) {
    return;
  }

  RefPtr<ReportingHeader> service = gReporting;
  gReporting = nullptr;

  if (service->mCleanupTimer) {
    service->mCleanupTimer->Cancel();
    service->mCleanupTimer = nullptr;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  obs->RemoveObserver(service, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC);
  obs->RemoveObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  obs->RemoveObserver(service, "clear-origin-attributes-data");
  obs->RemoveObserver(service, REPORTING_PURGE_HOST);
  obs->RemoveObserver(service, REPORTING_PURGE_ALL);
}

ReportingHeader::ReportingHeader() = default;
ReportingHeader::~ReportingHeader() = default;

NS_IMETHODIMP
ReportingHeader::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    return NS_OK;
  }

  // Pref disabled.
  if (!StaticPrefs::dom_reporting_header_enabled()) {
    return NS_OK;
  }

  if (!strcmp(aTopic, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC)) {
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!channel)) {
      return NS_OK;
    }

    ReportingFromChannel(channel);
    return NS_OK;
  }

  if (!strcmp(aTopic, REPORTING_PURGE_HOST)) {
    RemoveOriginsFromHost(nsDependentString(aData));
    return NS_OK;
  }

  if (!strcmp(aTopic, "clear-origin-attributes-data")) {
    OriginAttributesPattern pattern;
    if (!pattern.Init(nsDependentString(aData))) {
      NS_ERROR("Cannot parse origin attributes pattern");
      return NS_ERROR_FAILURE;
    }

    RemoveOriginsFromOriginAttributesPattern(pattern);
    return NS_OK;
  }

  if (!strcmp(aTopic, REPORTING_PURGE_ALL)) {
    RemoveOrigins();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void ReportingHeader::ReportingFromChannel(nsIHttpChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  if (!StaticPrefs::dom_reporting_header_enabled()) {
    return;
  }

  // We want to use the final URI to check if Report-To should be allowed or
  // not.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!IsSecureURI(uri)) {
    return;
  }

  if (NS_UsePrivateBrowsing(aChannel)) {
    return;
  }

  nsAutoCString headerValue;
  rv =
      aChannel->GetResponseHeader(NS_LITERAL_CSTRING("Report-To"), headerValue);
  if (NS_FAILED(rv)) {
    return;
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_WARN_IF(!ssm)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetChannelURIPrincipal(aChannel, getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv)) || !principal) {
    return;
  }

  nsAutoCString origin;
  rv = principal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  UniquePtr<Client> client = ParseHeader(aChannel, uri, headerValue);
  if (!client) {
    return;
  }

  // Here we override the previous data.
  mOrigins.Put(origin, client.release());

  MaybeCreateCleanupTimer();
}

/* static */ UniquePtr<ReportingHeader::Client> ReportingHeader::ParseHeader(
    nsIHttpChannel* aChannel, nsIURI* aURI, const nsACString& aHeaderValue) {
  MOZ_ASSERT(aURI);
  // aChannel can be null in gtest

  AutoJSAPI jsapi;

  JSObject* cleanGlobal =
      SimpleGlobalObject::Create(SimpleGlobalObject::GlobalType::BindingDetail);
  if (NS_WARN_IF(!cleanGlobal)) {
    return nullptr;
  }

  if (NS_WARN_IF(!jsapi.Init(cleanGlobal))) {
    return nullptr;
  }

  // WebIDL dictionary parses single items. Let's create a object to parse the
  // header.
  nsAutoString json;
  json.AppendASCII("{ \"items\": [");
  json.Append(NS_ConvertUTF8toUTF16(aHeaderValue));
  json.AppendASCII("]}");

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsonValue(cx);
  bool ok = JS_ParseJSON(cx, PromiseFlatString(json).get(), json.Length(),
                         &jsonValue);
  if (!ok) {
    LogToConsoleInvalidJSON(aChannel, aURI);
    return nullptr;
  }

  dom::ReportingHeaderValue data;
  if (!data.Init(cx, jsonValue)) {
    LogToConsoleInvalidJSON(aChannel, aURI);
    return nullptr;
  }

  if (!data.mItems.WasPassed() || data.mItems.Value().IsEmpty()) {
    return nullptr;
  }

  UniquePtr<Client> client = MakeUnique<Client>();

  for (const dom::ReportingItem& item : data.mItems.Value()) {
    nsAutoString groupName;

    if (item.mGroup.isUndefined()) {
      groupName.AssignLiteral("default");
    } else if (!item.mGroup.isString()) {
      LogToConsoleInvalidNameItem(aChannel, aURI);
      continue;
    } else {
      JS::Rooted<JSString*> groupStr(cx, item.mGroup.toString());
      MOZ_ASSERT(groupStr);

      nsAutoJSString string;
      if (NS_WARN_IF(!string.init(cx, groupStr))) {
        continue;
      }

      groupName = string;
    }

    if (!item.mMax_age.isNumber() || !item.mEndpoints.isObject()) {
      LogToConsoleIncompleteItem(aChannel, aURI, groupName);
      continue;
    }

    JS::Rooted<JSObject*> endpoints(cx, &item.mEndpoints.toObject());
    MOZ_ASSERT(endpoints);

    bool isArray = false;
    if (!JS::IsArrayObject(cx, endpoints, &isArray) || !isArray) {
      LogToConsoleIncompleteItem(aChannel, aURI, groupName);
      continue;
    }

    uint32_t endpointsLength;
    if (!JS::GetArrayLength(cx, endpoints, &endpointsLength) ||
        endpointsLength == 0) {
      LogToConsoleIncompleteItem(aChannel, aURI, groupName);
      continue;
    }

    bool found = false;
    nsTObserverArray<Group>::ForwardIterator iter(client->mGroups);
    while (iter.HasMore()) {
      const Group& group = iter.GetNext();

      if (group.mName == groupName) {
        found = true;
        break;
      }
    }

    if (found) {
      LogToConsoleDuplicateGroup(aChannel, aURI, groupName);
      continue;
    }

    Group* group = client->mGroups.AppendElement();
    group->mName = groupName;
    group->mIncludeSubdomains = item.mInclude_subdomains;
    group->mTTL = item.mMax_age.toNumber();
    group->mCreationTime = TimeStamp::Now();

    for (uint32_t i = 0; i < endpointsLength; ++i) {
      JS::Rooted<JS::Value> element(cx);
      if (!JS_GetElement(cx, endpoints, i, &element)) {
        return nullptr;
      }

      RootedDictionary<ReportingEndpoint> endpoint(cx);
      if (!endpoint.Init(cx, element)) {
        LogToConsoleIncompleteEndpoint(aChannel, aURI, groupName);
        continue;
      }

      if (!endpoint.mUrl.isString() ||
          (!endpoint.mPriority.isUndefined() &&
           (!endpoint.mPriority.isNumber() ||
            endpoint.mPriority.toNumber() < 0)) ||
          (!endpoint.mWeight.isUndefined() &&
           (!endpoint.mWeight.isNumber() || endpoint.mWeight.toNumber() < 0))) {
        LogToConsoleIncompleteEndpoint(aChannel, aURI, groupName);
        continue;
      }

      JS::Rooted<JSString*> endpointUrl(cx, endpoint.mUrl.toString());
      MOZ_ASSERT(endpointUrl);

      nsAutoJSString endpointString;
      if (NS_WARN_IF(!endpointString.init(cx, endpointUrl))) {
        continue;
      }

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), endpointString);
      if (NS_FAILED(rv)) {
        LogToConsoleInvalidURLEndpoint(aChannel, aURI, groupName,
                                       endpointString);
        continue;
      }

      Endpoint* ep = group->mEndpoints.AppendElement();
      ep->mUrl = uri;
      ep->mPriority =
          endpoint.mPriority.isUndefined() ? 1 : endpoint.mPriority.toNumber();
      ep->mWeight =
          endpoint.mWeight.isUndefined() ? 1 : endpoint.mWeight.toNumber();
    }
  }

  if (client->mGroups.IsEmpty()) {
    return nullptr;
  }

  return client;
}

bool ReportingHeader::IsSecureURI(nsIURI* aURI) const {
  MOZ_ASSERT(aURI);

  bool prioriAuthenticated = false;
  if (NS_WARN_IF(NS_FAILED(NS_URIChainHasFlags(
          aURI, nsIProtocolHandler::URI_IS_POTENTIALLY_TRUSTWORTHY,
          &prioriAuthenticated)))) {
    return false;
  }

  return prioriAuthenticated;
}

/* static */
void ReportingHeader::LogToConsoleInvalidJSON(nsIHttpChannel* aChannel,
                                              nsIURI* aURI) {
  nsTArray<nsString> params;
  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderInvalidJSON", params);
}

/* static */
void ReportingHeader::LogToConsoleDuplicateGroup(nsIHttpChannel* aChannel,
                                                 nsIURI* aURI,
                                                 const nsAString& aName) {
  nsTArray<nsString> params;
  params.AppendElement(aName);

  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderDuplicateGroup", params);
}

/* static */
void ReportingHeader::LogToConsoleInvalidNameItem(nsIHttpChannel* aChannel,
                                                  nsIURI* aURI) {
  nsTArray<nsString> params;
  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderInvalidNameItem",
                       params);
}

/* static */
void ReportingHeader::LogToConsoleIncompleteItem(nsIHttpChannel* aChannel,
                                                 nsIURI* aURI,
                                                 const nsAString& aName) {
  nsTArray<nsString> params;
  params.AppendElement(aName);

  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderInvalidItem", params);
}

/* static */
void ReportingHeader::LogToConsoleIncompleteEndpoint(nsIHttpChannel* aChannel,
                                                     nsIURI* aURI,
                                                     const nsAString& aName) {
  nsTArray<nsString> params;
  params.AppendElement(aName);

  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderInvalidEndpoint",
                       params);
}

/* static */
void ReportingHeader::LogToConsoleInvalidURLEndpoint(nsIHttpChannel* aChannel,
                                                     nsIURI* aURI,
                                                     const nsAString& aName,
                                                     const nsAString& aURL) {
  nsTArray<nsString> params;
  params.AppendElement(aURL);
  params.AppendElement(aName);

  LogToConsoleInternal(aChannel, aURI, "ReportingHeaderInvalidURLEndpoint",
                       params);
}

/* static */
void ReportingHeader::LogToConsoleInternal(nsIHttpChannel* aChannel,
                                           nsIURI* aURI, const char* aMsg,
                                           const nsTArray<nsString>& aParams) {
  MOZ_ASSERT(aURI);

  if (!aChannel) {
    // We are in a gtest.
    return;
  }

  uint64_t windowID = 0;

  nsresult rv = aChannel->GetTopLevelContentWindowId(&windowID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!windowID) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsresult rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (loadGroup) {
      windowID = nsContentUtils::GetInnerWindowID(loadGroup);
    }
  }

  nsAutoString localizedMsg;
  rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES, aMsg, aParams, localizedMsg);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = nsContentUtils::ReportToConsoleByWindowID(
      localizedMsg, nsIScriptError::infoFlag, NS_LITERAL_CSTRING("Reporting"),
      windowID, aURI);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

/* static */
void ReportingHeader::GetEndpointForReport(
    const nsAString& aGroupName,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    nsACString& aEndpointURI) {
  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);
  if (NS_WARN_IF(!principal)) {
    return;
  }
  GetEndpointForReport(aGroupName, principal, aEndpointURI);
}

/* static */
void ReportingHeader::GetEndpointForReport(const nsAString& aGroupName,
                                           nsIPrincipal* aPrincipal,
                                           nsACString& aEndpointURI) {
  MOZ_ASSERT(aEndpointURI.IsEmpty());

  if (!gReporting) {
    return;
  }

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  Client* client = gReporting->mOrigins.Get(origin);
  if (!client) {
    return;
  }

  nsTObserverArray<Group>::ForwardIterator iter(client->mGroups);
  while (iter.HasMore()) {
    const Group& group = iter.GetNext();

    if (group.mName == aGroupName) {
      GetEndpointForReportInternal(group, aEndpointURI);
      break;
    }
  }
}

/* static */
void ReportingHeader::GetEndpointForReportInternal(
    const ReportingHeader::Group& aGroup, nsACString& aEndpointURI) {
  TimeDuration diff = TimeStamp::Now() - aGroup.mCreationTime;
  if (diff.ToSeconds() > aGroup.mTTL) {
    // Expired.
    return;
  }

  if (aGroup.mEndpoints.IsEmpty()) {
    return;
  }

  int64_t minPriority = -1;
  uint32_t totalWeight = 0;

  nsTObserverArray<Endpoint>::ForwardIterator iter(aGroup.mEndpoints);
  while (iter.HasMore()) {
    const Endpoint& endpoint = iter.GetNext();

    if (minPriority == -1 || minPriority > endpoint.mPriority) {
      minPriority = endpoint.mPriority;
      totalWeight = endpoint.mWeight;
    } else if (minPriority == endpoint.mPriority) {
      totalWeight += endpoint.mWeight;
    }
  }

  nsCOMPtr<nsIRandomGenerator> randomGenerator =
      do_GetService("@mozilla.org/security/random-generator;1");
  if (NS_WARN_IF(!randomGenerator)) {
    return;
  }

  uint32_t randomNumber = 0;

  uint8_t* buffer;
  nsresult rv =
      randomGenerator->GenerateRandomBytes(sizeof(randomNumber), &buffer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  memcpy(&randomNumber, buffer, sizeof(randomNumber));
  free(buffer);

  totalWeight = randomNumber % totalWeight;

  nsTObserverArray<Endpoint>::ForwardIterator iter2(aGroup.mEndpoints);
  while (iter2.HasMore()) {
    const Endpoint& endpoint = iter2.GetNext();

    if (minPriority == endpoint.mPriority && totalWeight < endpoint.mWeight) {
      Unused << NS_WARN_IF(NS_FAILED(endpoint.mUrl->GetSpec(aEndpointURI)));
      break;
    }
  }
}

/* static */
void ReportingHeader::RemoveEndpoint(
    const nsAString& aGroupName, const nsACString& aEndpointURL,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  if (!gReporting) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aEndpointURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);
  if (NS_WARN_IF(!principal)) {
    return;
  }

  nsAutoCString origin;
  rv = principal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  Client* client = gReporting->mOrigins.Get(origin);
  if (!client) {
    return;
  }

  // Scope for the group iterator.
  {
    nsTObserverArray<Group>::BackwardIterator iter(client->mGroups);
    while (iter.HasMore()) {
      const Group& group = iter.GetNext();
      if (group.mName != aGroupName) {
        continue;
      }

      // Scope for the endpoint iterator.
      {
        nsTObserverArray<Endpoint>::BackwardIterator endpointIter(
            group.mEndpoints);
        while (endpointIter.HasMore()) {
          const Endpoint& endpoint = endpointIter.GetNext();

          bool equal = false;
          rv = endpoint.mUrl->Equals(uri, &equal);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            continue;
          }

          if (equal) {
            endpointIter.Remove();
            break;
          }
        }
      }

      if (group.mEndpoints.IsEmpty()) {
        iter.Remove();
      }

      break;
    }
  }

  if (client->mGroups.IsEmpty()) {
    gReporting->mOrigins.Remove(origin);
    gReporting->MaybeCancelCleanupTimer();
  }
}

void ReportingHeader::RemoveOriginsFromHost(const nsAString& aHost) {
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (NS_WARN_IF(!tldService)) {
    return;
  }

  NS_ConvertUTF16toUTF8 host(aHost);

  for (auto iter = mOrigins.Iter(); !iter.Done(); iter.Next()) {
    bool hasRootDomain = false;
    nsresult rv = tldService->HasRootDomain(iter.Key(), host, &hasRootDomain);
    if (NS_WARN_IF(NS_FAILED(rv)) || !hasRootDomain) {
      continue;
    }

    iter.Remove();
  }

  MaybeCancelCleanupTimer();
}

void ReportingHeader::RemoveOriginsFromOriginAttributesPattern(
    const OriginAttributesPattern& aPattern) {
  for (auto iter = mOrigins.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString suffix;
    OriginAttributes attr;
    if (NS_WARN_IF(!attr.PopulateFromOrigin(iter.Key(), suffix))) {
      continue;
    }

    if (aPattern.Matches(attr)) {
      iter.Remove();
    }
  }

  MaybeCancelCleanupTimer();
}

void ReportingHeader::RemoveOrigins() {
  mOrigins.Clear();
  MaybeCancelCleanupTimer();
}

void ReportingHeader::RemoveOriginsForTTL() {
  TimeStamp now = TimeStamp::Now();

  for (auto iter = mOrigins.Iter(); !iter.Done(); iter.Next()) {
    Client* client = iter.UserData();

    // Scope of the iterator.
    {
      nsTObserverArray<Group>::BackwardIterator groupIter(client->mGroups);
      while (groupIter.HasMore()) {
        const Group& group = groupIter.GetNext();
        TimeDuration diff = now - group.mCreationTime;
        if (diff.ToSeconds() > group.mTTL) {
          groupIter.Remove();
          return;
        }
      }
    }

    if (client->mGroups.IsEmpty()) {
      iter.Remove();
    }
  }
}

/* static */
bool ReportingHeader::HasReportingHeaderForOrigin(const nsACString& aOrigin) {
  if (!gReporting) {
    return false;
  }

  return gReporting->mOrigins.Contains(aOrigin);
}

NS_IMETHODIMP
ReportingHeader::Notify(nsITimer* aTimer) {
  mCleanupTimer = nullptr;

  RemoveOriginsForTTL();
  MaybeCreateCleanupTimer();

  return NS_OK;
}

void ReportingHeader::MaybeCreateCleanupTimer() {
  if (mCleanupTimer) {
    return;
  }

  if (mOrigins.Count() == 0) {
    return;
  }

  uint32_t timeout = StaticPrefs::dom_reporting_cleanup_timeout() * 1000;
  nsresult rv =
      NS_NewTimerWithCallback(getter_AddRefs(mCleanupTimer), this, timeout,
                              nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void ReportingHeader::MaybeCancelCleanupTimer() {
  if (!mCleanupTimer) {
    return;
  }

  if (mOrigins.Count() != 0) {
    return;
  }

  mCleanupTimer->Cancel();
  mCleanupTimer = nullptr;
}

NS_INTERFACE_MAP_BEGIN(ReportingHeader)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(ReportingHeader)
NS_IMPL_RELEASE(ReportingHeader)

}  // namespace dom
}  // namespace mozilla
