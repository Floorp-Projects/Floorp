/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GVAutoplayPermissionRequest.h"

#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {
namespace dom {

using RType = GVAutoplayRequestType;
using RStatus = GVAutoplayRequestStatus;

static RStatus GetRequestStatus(BrowsingContext* aContext, RType aType) {
  MOZ_ASSERT(aContext);
  AssertIsOnMainThread();
  return aType == RType::eAUDIBLE
             ? aContext->GetGVAudibleAutoplayRequestStatus()
             : aContext->GetGVInaudibleAutoplayRequestStatus();
}

// This is copied from the value of `media.geckoview.autoplay.request.testing`.
enum class TestRequest : uint32_t {
  ePromptAsNormal = 0,
  eAllowAll = 1,
  eDenyAll = 2,
  eAllowAudible = 3,
  eDenyAudible = 4,
  eAllowInAudible = 5,
  eDenyInAudible = 6,
  eLeaveAllPending = 7,
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(GVAutoplayPermissionRequest,
                                   ContentPermissionRequestBase)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(GVAutoplayPermissionRequest,
                                               ContentPermissionRequestBase)

/* static */
void GVAutoplayPermissionRequest::CreateRequest(nsGlobalWindowInner* aWindow,
                                                BrowsingContext* aContext,
                                                GVAutoplayRequestType aType) {
  RefPtr<GVAutoplayPermissionRequest> request =
      new GVAutoplayPermissionRequest(aWindow, aContext, aType);
  request->SetRequestStatus(RStatus::ePENDING);
  const TestRequest testingPref = static_cast<TestRequest>(
      StaticPrefs::media_geckoview_autoplay_request_testing());
  if (testingPref != TestRequest::ePromptAsNormal) {
    if (testingPref == TestRequest::eAllowAll ||
        (testingPref == TestRequest::eAllowAudible &&
         aType == RType::eAUDIBLE) ||
        (testingPref == TestRequest::eAllowInAudible &&
         aType == RType::eINAUDIBLE)) {
      request->Allow(JS::UndefinedHandleValue);
    } else if (testingPref == TestRequest::eDenyAll ||
               (testingPref == TestRequest::eDenyAudible &&
                aType == RType::eAUDIBLE) ||
               (testingPref == TestRequest::eDenyInAudible &&
                aType == RType::eINAUDIBLE)) {
      request->Cancel();
    }
  } else {
    request->RequestDelayedTask(
        aWindow->EventTargetFor(TaskCategory::Other),
        GVAutoplayPermissionRequest::DelayedTaskType::Request);
  }
}

GVAutoplayPermissionRequest::GVAutoplayPermissionRequest(
    nsGlobalWindowInner* aWindow, BrowsingContext* aContext, RType aType)
    : ContentPermissionRequestBase(
          aWindow->GetPrincipal(), aWindow,
          NS_LITERAL_CSTRING(""),  // No testing pref used in this class
          aType == RType::eAUDIBLE
              ? NS_LITERAL_CSTRING("autoplay-media-audible")
              : NS_LITERAL_CSTRING("autoplay-media-inaudible")),
      mType(aType),
      mContext(aContext) {
  MOZ_ASSERT(mContext);
}

GVAutoplayPermissionRequest::~GVAutoplayPermissionRequest() {
  // If user doesn't response to the request before it gets destroyed (ex.
  // request dismissed, tab closed, naviagation to a new page), then we should
  // treat it as a denial.
  if (mContext) {
    Cancel();
  }
}

void GVAutoplayPermissionRequest::SetRequestStatus(RStatus aStatus) {
  MOZ_ASSERT(mContext);
  AssertIsOnMainThread();
  if (mType == RType::eAUDIBLE) {
    mContext->SetGVAudibleAutoplayRequestStatus(aStatus);
  } else {
    mContext->SetGVInaudibleAutoplayRequestStatus(aStatus);
  }
}

NS_IMETHODIMP
GVAutoplayPermissionRequest::Cancel() {
  MOZ_ASSERT(mContext, "Do not call 'Cancel()' twice!");
  // As the process of replying of the request is an async task, the status
  // might have be reset at the time we get the result from parent process.
  // Ex. if the page got closed or naviagated immediately after user replied to
  // the request. Therefore, the status should be either `pending` or `unknown`.
  const RStatus status = GetRequestStatus(mContext, mType);
  MOZ_ASSERT(status == RStatus::ePENDING || status == RStatus::eUNKNOWN);
  if (status == RStatus::ePENDING) {
    SetRequestStatus(RStatus::eDENIED);
  }
  mContext = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
GVAutoplayPermissionRequest::Allow(JS::HandleValue aChoices) {
  MOZ_ASSERT(mContext, "Do not call 'Allow()' twice!");
  // As the process of replying of the request is an async task, the status
  // might have be reset at the time we get the result from parent process.
  // Ex. if the page got closed or naviagated immediately after user replied to
  // the request. Therefore, the status should be either `pending` or `unknown`.
  const RStatus status = GetRequestStatus(mContext, mType);
  MOZ_ASSERT(status == RStatus::ePENDING || status == RStatus::eUNKNOWN);
  if (status == RStatus::ePENDING) {
    SetRequestStatus(RStatus::eALLOWED);
  }
  mContext = nullptr;
  return NS_OK;
}

/* static */
void GVAutoplayPermissionRequestor::AskForPermissionIfNeeded(
    nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return;
  }

  // The request is used for content permission, so it's no need to create a
  // content request in parent process.
  if (XRE_IsParentProcess()) {
    return;
  }

  if (!StaticPrefs::media_geckoview_autoplay_request()) {
    return;
  }

  // The request status is stored in top-level browsing context only.
  RefPtr<BrowsingContext> context = aWindow->GetBrowsingContext()->Top();
  if (!HasEverAskForRequest(context, RType::eAUDIBLE)) {
    CreateAsyncRequest(aWindow, context, RType::eAUDIBLE);
  }
  if (!HasEverAskForRequest(context, RType::eINAUDIBLE)) {
    CreateAsyncRequest(aWindow, context, RType::eINAUDIBLE);
  }
}

/* static */
bool GVAutoplayPermissionRequestor::HasEverAskForRequest(
    BrowsingContext* aContext, RType aType) {
  return GetRequestStatus(aContext, aType) != RStatus::eUNKNOWN;
}

/* static */
void GVAutoplayPermissionRequestor::CreateAsyncRequest(
    nsPIDOMWindowInner* aWindow, BrowsingContext* aContext,
    GVAutoplayRequestType aType) {
  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  if (!innerWindow || !innerWindow->GetPrincipal()) {
    return;
  }

  GVAutoplayPermissionRequest::CreateRequest(innerWindow, aContext, aType);
}

}  // namespace dom
}  // namespace mozilla
