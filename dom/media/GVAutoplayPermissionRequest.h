/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GVAUTOPLAYPERMISSIONREQUEST_H_
#define DOM_MEDIA_GVAUTOPLAYPERMISSIONREQUEST_H_

#include "GVAutoplayRequestUtils.h"
#include "nsContentPermissionHelper.h"

namespace mozilla {
namespace dom {

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

  static already_AddRefed<GVAutoplayPermissionRequest> CreateRequest(
      nsGlobalWindowInner* aWindow, GVAutoplayRequestType aType);

  // nsIContentPermissionRequest methods
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::HandleValue choices) override;

 private:
  GVAutoplayPermissionRequest(nsGlobalWindowInner* aWindow,
                              GVAutoplayRequestType aType);
  ~GVAutoplayPermissionRequest();

  GVAutoplayRequestType mType;
};

}  // namespace dom
}  // namespace mozilla

#endif
