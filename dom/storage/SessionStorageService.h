/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageService_h
#define mozilla_dom_SessionStorageService_h

#include "mozilla/UniquePtr.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsPointerHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace dom {

class SessionStorageManager;

class SessionStorageService final : public nsIObserver {
 public:
  SessionStorageService();

  static SessionStorageService* Get();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void RegisterSessionStorageManager(SessionStorageManager* aManager);
  void UnregisterSessionStorageManager(SessionStorageManager* aManager);

 private:
  ~SessionStorageService();

  static void ShutDown();

  void SendSessionStorageDataToParentProcess();

  static RefPtr<SessionStorageService> sService;
  static bool sShutdown;

  nsTHashtable<nsPtrHashKey<SessionStorageManager>> mManagers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStorageService_h
