/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAPERMISSIONGONK_H
#define DOM_MEDIA_MEDIAPERMISSIONGONK_H

#include "nsError.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "GetUserMediaRequest.h"

namespace mozilla {

/**
 * The observer to create the MediaPermissionMgr. This is the entry point of
 * permission request on b2g.
 */
class MediaPermissionManager : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static MediaPermissionManager* GetInstance();
  virtual ~MediaPermissionManager();

private:
  MediaPermissionManager();
  nsresult Deinit();
  nsresult HandleRequest(nsRefPtr<dom::GetUserMediaRequest> &req);
};

} // namespace mozilla
#endif // DOM_MEDIA_MEDIAPERMISSIONGONK_H

