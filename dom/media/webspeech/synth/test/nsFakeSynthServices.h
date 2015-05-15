/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFakeSynthServices_h
#define nsFakeSynthServices_h

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsISpeechService.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Monitor.h"

namespace mozilla {
namespace dom {

class nsFakeSynthServices : public nsIObserver
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFakeSynthServices();

  static nsFakeSynthServices* GetInstance();

  static already_AddRefed<nsFakeSynthServices> GetInstanceForService();

  static void Shutdown();

private:

  virtual ~nsFakeSynthServices();

  void Init();

  nsCOMPtr<nsISpeechService> mDirectService;

  nsCOMPtr<nsISpeechService> mIndirectService;

  static StaticRefPtr<nsFakeSynthServices> sSingleton;
};

} // namespace dom
} // namespace mozilla

#endif
