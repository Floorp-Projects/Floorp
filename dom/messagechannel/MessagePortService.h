/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortService_h
#define mozilla_dom_MessagePortService_h

#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class MessagePortParent;
class SharedMessagePortMessage;

class MessagePortService final
{
public:
  NS_INLINE_DECL_REFCOUNTING(MessagePortService)

  static MessagePortService* Get();
  static MessagePortService* GetOrCreate();

  bool RequestEntangling(MessagePortParent* aParent,
                         const nsID& aDestinationUUID,
                         const uint32_t& aSequenceID);

  bool DisentanglePort(
                 MessagePortParent* aParent,
                 FallibleTArray<RefPtr<SharedMessagePortMessage>>& aMessages);

  bool ClosePort(MessagePortParent* aParent);

  bool PostMessages(
                 MessagePortParent* aParent,
                 FallibleTArray<RefPtr<SharedMessagePortMessage>>& aMessages);

  void ParentDestroy(MessagePortParent* aParent);

  bool ForceClose(const nsID& aUUID,
                  const nsID& aDestinationUUID,
                  const uint32_t& aSequenceID);

private:
  ~MessagePortService() {}

  void CloseAll(const nsID& aUUID, bool aForced = false);
  void MaybeShutdown();

  class MessagePortServiceData;

#ifdef DEBUG
  static PLDHashOperator
  CloseAllDebugCheck(const nsID& aID, MessagePortServiceData* aData,
                     void* aPtr);
#endif

  nsClassHashtable<nsIDHashKey, MessagePortServiceData> mPorts;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortService_h
