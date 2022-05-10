/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortService_h
#define mozilla_dom_MessagePortService_h

#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class MessagePortParent;
class SharedMessageBody;

class MessagePortService final {
 public:
  NS_INLINE_DECL_REFCOUNTING(MessagePortService)

  // Needs to be public for the MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR
  // macro.
  struct NextParent;

  static MessagePortService* Get();
  static MessagePortService* GetOrCreate();

  bool RequestEntangling(MessagePortParent* aParent,
                         const nsID& aDestinationUUID,
                         const uint32_t& aSequenceID);

  bool DisentanglePort(MessagePortParent* aParent,
                       FallibleTArray<RefPtr<SharedMessageBody>> aMessages);

  bool ClosePort(MessagePortParent* aParent);

  bool PostMessages(MessagePortParent* aParent,
                    FallibleTArray<RefPtr<SharedMessageBody>> aMessages);

  void ParentDestroy(MessagePortParent* aParent);

  bool ForceClose(const nsID& aUUID, const nsID& aDestinationUUID,
                  const uint32_t& aSequenceID);

 private:
  ~MessagePortService() = default;

  void CloseAll(const nsID& aUUID, bool aForced = false);
  void MaybeShutdown();

  class MessagePortServiceData;

  nsClassHashtable<nsIDHashKey, MessagePortServiceData> mPorts;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MessagePortService_h
