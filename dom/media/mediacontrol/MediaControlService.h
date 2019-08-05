/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediacontrolservice_h__
#define mozilla_dom_mediacontrolservice_h__

#include "mozilla/AlreadyAddRefed.h"

#include "MediaController.h"
#include "nsDataHashtable.h"
#include "nsIObserver.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlService is an interface to access controllers by providing
 * controller Id. Everytime when controller becomes active, which means there is
 * one or more media started in the corresponding browsing context, so now the
 * controller is actually controlling something in the content process, so it
 * would be added into the list of the MediaControlService. The controller would
 * be removed from the list of the MediaControlService when it becomes inactive,
 * which means no media is playing in the corresponding browsing context. Note
 * that, a controller can't be added to or remove from the list twice. It should
 * should have a responsibility to add and remove itself in the proper time.
 */
class MediaControlService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static RefPtr<MediaControlService> GetService();

  RefPtr<MediaController> GetOrCreateControllerById(const uint64_t aId) const;
  RefPtr<MediaController> GetControllerById(const uint64_t aId) const;

  void AddMediaController(const RefPtr<MediaController>& aController);
  void RemoveMediaController(const RefPtr<MediaController>& aController);
  uint64_t GetControllersNum() const;

 private:
  MediaControlService();
  ~MediaControlService();

  void PlayAllControllers() const;
  void PauseAllControllers() const;
  void StopAllControllers() const;
  void ShutdownAllControllers() const;

  nsDataHashtable<nsUint64HashKey, RefPtr<MediaController>> mControllers;
};

}  // namespace dom
}  // namespace mozilla

#endif
