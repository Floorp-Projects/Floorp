/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcService_h
#define NfcService_h

#include "mozilla/ipc/Nfc.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsCOMPtr.h"
#include "nsINfcService.h"
#include "NfcMessageHandler.h"

class nsIThread;

namespace mozilla {
namespace dom {
class NfcEventOptions;
} // namespace dom

class NfcService MOZ_FINAL : public nsINfcService,
                             public mozilla::ipc::NfcSocketListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINFCSERVICE

  static already_AddRefed<NfcService> FactoryCreate();

  void DispatchNfcEvent(const mozilla::dom::NfcEventOptions& aOptions);

  virtual void
  ReceiveSocketData(nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aData) MOZ_OVERRIDE;

  nsCOMPtr<nsIThread> GetThread() {
    return mThread;
  }

private:
  NfcService();
  ~NfcService();

  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<nsINfcEventListener> mListener;
  nsRefPtr<mozilla::ipc::NfcConsumer> mConsumer;
  nsAutoPtr<NfcMessageHandler> mHandler;
};

} // namespace mozilla

#endif // NfcService_h
