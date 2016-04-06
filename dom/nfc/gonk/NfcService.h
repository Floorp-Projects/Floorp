/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcService_h
#define NfcService_h

#include "nsCOMPtr.h"
#include "nsINfcService.h"

class nsIThread;

namespace mozilla {
namespace dom {
class NfcEventOptions;
} // namespace dom

class NfcConsumer;

class NfcService final : public nsINfcService
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINFCSERVICE

  static already_AddRefed<NfcService> FactoryCreate();

  void DispatchNfcEvent(const mozilla::dom::NfcEventOptions& aOptions);

private:
  class CleanupRunnable;
  class SendRunnable;
  class ShutdownConsumerRunnable;
  class StartConsumerRunnable;

  NfcService();
  ~NfcService();

  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<nsINfcGonkEventListener> mListener;
  UniquePtr<NfcConsumer> mNfcConsumer;
};

} // namespace mozilla

#endif // NfcService_h
