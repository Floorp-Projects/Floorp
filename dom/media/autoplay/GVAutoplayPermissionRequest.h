/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GVAUTOPLAYPERMISSIONREQUEST_H_
#define DOM_MEDIA_GVAUTOPLAYPERMISSIONREQUEST_H_

#include "GVAutoplayRequestUtils.h"
#include "nsContentPermissionHelper.h"

class nsGlobalWindowInner;

namespace mozilla::dom {

/**
 * This class is used to provide an ability for GeckoView (GV) to allow its
 * embedder (application) to decide whether the autoplay media should be allowed
 * or denied on the page. We have two types of request, one for audible media,
 * another one for inaudible media. Each page would at most have one request per
 * type at a time, and the result of request would be effective on that page
 * until the page gets reloaded or closed.
 */
class GVAutoplayPermissionRequest : public ContentPermissionRequestBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GVAutoplayPermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest methods
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;

 private:
  // Only allow to create this request from the requestor.
  friend class GVAutoplayPermissionRequestor;
  static void CreateRequest(nsGlobalWindowInner* aWindow,
                            BrowsingContext* aContext,
                            GVAutoplayRequestType aType);

  GVAutoplayPermissionRequest(nsGlobalWindowInner* aWindow,
                              BrowsingContext* aContext,
                              GVAutoplayRequestType aType);
  ~GVAutoplayPermissionRequest();

  void SetRequestStatus(GVAutoplayRequestStatus aStatus);

  GVAutoplayRequestType mType;
  RefPtr<BrowsingContext> mContext;
};

/**
 * This class provides a method to request autoplay permission for a page, which
 * would be used to be a factor to determine if media is allowed to autoplay or
 * not on GeckoView.
 *
 * A page could only have at most one audible request and one inaudible request,
 * and once a page has been closed or reloaded, those requests would be dropped.
 * In order to achieve that all media existing in the same page can share the
 * result of those requests, the request status would only be stored in the
 * top-level browsing context, which allows them to be synchronized among
 * different processes when Fission is enabled.
 *
 * The current way we choose is to request for a permission when creating media
 * element, in order to get the response from the embedding app before media
 * starts playing if the app can response the request quickly enough. However,
 * the request might be pending if the app doesn't response to it, we might
 * never get the response. As that is just one factor of determining the
 * autoplay result, even if we don't get the response for the request, we still
 * have a chance to play media. Check AutoplayPolicy to see more details about
 * how we decide the final autoplay decision.
 */
class GVAutoplayPermissionRequestor final {
 public:
  static void AskForPermissionIfNeeded(nsPIDOMWindowInner* aWindow);

 private:
  static bool HasEverAskForRequest(BrowsingContext* aContext,
                                   GVAutoplayRequestType aType);
  static void CreateAsyncRequest(nsPIDOMWindowInner* aWindow,
                                 BrowsingContext* aContext,
                                 GVAutoplayRequestType aType);
};

}  // namespace mozilla::dom

#endif
