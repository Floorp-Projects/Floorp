/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_system_b2g_systemworkermanager_h__
#define mozilla_dom_system_b2g_systemworkermanager_h__

#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDOMEventTargetHelper.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIWorkerHolder;

namespace mozilla {
namespace dom {
namespace gonk {

class SystemWorkerManager : public nsIObserver,
                            public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR

  nsresult Init();
  void Shutdown();

  static already_AddRefed<SystemWorkerManager>
  FactoryCreate();

  static nsIInterfaceRequestor*
  GetInterfaceRequestor();

private:
  SystemWorkerManager();
  ~SystemWorkerManager();

  nsresult InitRIL(JSContext *cx);
  nsresult InitWifi(JSContext *cx);

  nsCOMPtr<nsIWorkerHolder> mRILWorker;
  nsCOMPtr<nsIWorkerHolder> mWifiWorker;

  bool mShutdown;
};

}
}
}

#endif // mozilla_dom_system_b2g_systemworkermanager_h__
