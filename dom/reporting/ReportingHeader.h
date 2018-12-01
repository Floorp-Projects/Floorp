/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReportingHeader_h
#define mozilla_dom_ReportingHeader_h

#include "mozilla/TimeStamp.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsTArray.h"

class nsIHttpChannel;
class nsIPrincipal;
class nsIURI;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}

namespace dom {

class ReportingHeader final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Initialize();

  // Exposed structs for gtests

  struct Endpoint {
    nsCOMPtr<nsIURI> mUrl;
    uint32_t mPriority;
    uint32_t mWeight;
  };

  struct Group {
    nsString mName;
    bool mIncludeSubdomains;
    int32_t mTTL;
    TimeStamp mCreationTime;
    nsTArray<Endpoint> mEndpoints;
  };

  struct Client {
    nsTArray<Group> mGroups;
  };

  static UniquePtr<Client> ParseHeader(nsIHttpChannel* aChannel, nsIURI* aURI,
                                       const nsACString& aHeaderValue);

  static void GetEndpointForReport(
      const nsAString& aGroupName,
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
      nsACString& aEndpointURI);

 private:
  ReportingHeader();
  ~ReportingHeader();

  static void Shutdown();

  // Checks if a channel contains a Report-To header and parses its value.
  void ReportingFromChannel(nsIHttpChannel* aChannel);

  // This method checks if the protocol handler of the URI has the
  // URI_IS_POTENTIALLY_TRUSTWORTHY flag.
  bool IsSecureURI(nsIURI* aURI) const;

  static void LogToConsoleInvalidJSON(nsIHttpChannel* aChannel, nsIURI* aURI);

  static void LogToConsoleDuplicateGroup(nsIHttpChannel* aChannel, nsIURI* aURI,
                                         const nsAString& aName);

  static void LogToConsoleInvalidNameItem(nsIHttpChannel* aChannel,
                                          nsIURI* aURI);

  static void LogToConsoleIncompleteItem(nsIHttpChannel* aChannel, nsIURI* aURI,
                                         const nsAString& aName);

  static void LogToConsoleIncompleteEndpoint(nsIHttpChannel* aChannel,
                                             nsIURI* aURI,
                                             const nsAString& aName);

  static void LogToConsoleInvalidURLEndpoint(nsIHttpChannel* aChannel,
                                             nsIURI* aURI,
                                             const nsAString& aName,
                                             const nsAString& aURL);

  static void LogToConsoleInternal(nsIHttpChannel* aChannel, nsIURI* aURI,
                                   const char* aMsg,
                                   const nsTArray<nsString>& aParams);

  static void GetEndpointForReportInternal(const ReportingHeader::Group& aGrup,
                                           nsACString& aEndpointURI);

  nsClassHashtable<nsCStringHashKey, Client> mOrigins;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReportingHeader_h
