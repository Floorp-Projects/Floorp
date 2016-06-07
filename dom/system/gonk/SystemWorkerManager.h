/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_dom_system_b2g_systemworkermanager_h__
#define mozilla_dom_system_b2g_systemworkermanager_h__

#include "nsIInterfaceRequestor.h"
#include "nsISystemWorkerManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsXULAppAPI.h" // For XRE_GetProcessType

class nsIWorkerHolder;

namespace mozilla {

namespace ipc {
  class KeyStore;
}

namespace dom {
namespace gonk {

class SystemWorkerManager final : public nsIObserver,
                                  public nsIInterfaceRequestor,
                                  public nsISystemWorkerManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISYSTEMWORKERMANAGER

  nsresult Init();
  void Shutdown();

  static already_AddRefed<SystemWorkerManager>
  FactoryCreate();

  static nsIInterfaceRequestor*
  GetInterfaceRequestor();

private:
  SystemWorkerManager();
  ~SystemWorkerManager();

  nsresult InitWifi();
  nsresult InitKeyStore();

  nsCOMPtr<nsIWorkerHolder> mWifiWorker;

  RefPtr<mozilla::ipc::KeyStore> mKeyStore;

  bool mShutdown;
};

}
}
}

#endif // mozilla_dom_system_b2g_systemworkermanager_h__
