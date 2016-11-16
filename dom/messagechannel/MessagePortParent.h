/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortParent_h
#define mozilla_dom_MessagePortParent_h

#include "mozilla/dom/PMessagePortParent.h"

namespace mozilla {
namespace dom {

class MessagePortService;

class MessagePortParent final : public PMessagePortParent
{
public:
  explicit MessagePortParent(const nsID& aUUID);
  ~MessagePortParent();

  bool Entangle(const nsID& aDestinationUUID,
                const uint32_t& aSequenceID);

  bool Entangled(const nsTArray<MessagePortMessage>& aMessages);

  void Close();
  void CloseAndDelete();

  bool CanSendData() const
  {
    return mCanSendData;
  }

  const nsID& ID() const
  {
    return mUUID;
  }

  static bool ForceClose(const nsID& aUUID,
                         const nsID& aDestinationUUID,
                         const uint32_t& aSequenceID);

private:
  virtual mozilla::ipc::IPCResult RecvPostMessages(nsTArray<MessagePortMessage>&& aMessages)
                                                                       override;

  virtual mozilla::ipc::IPCResult RecvDisentangle(nsTArray<MessagePortMessage>&& aMessages)
                                                                       override;

  virtual mozilla::ipc::IPCResult RecvStopSendingData() override;

  virtual mozilla::ipc::IPCResult RecvClose() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<MessagePortService> mService;
  const nsID mUUID;
  bool mEntangled;
  bool mCanSendData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortParent_h
