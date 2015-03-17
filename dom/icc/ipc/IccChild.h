/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccChild_h
#define mozilla_dom_icc_IccChild_h

#include "mozilla/dom/icc/PIccChild.h"
#include "mozilla/dom/icc/PIccRequestChild.h"
#include "nsIIccService.h"

namespace mozilla {
namespace dom {

class IccInfo;

namespace icc {

class IccChild MOZ_FINAL : public PIccChild
                         , public nsIIcc
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICC

  explicit IccChild();

  void
  Init();

  void
  Shutdown();

protected:
  virtual void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual PIccRequestChild*
  AllocPIccRequestChild(const IccRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPIccRequestChild(PIccRequestChild* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyCardStateChanged(const uint32_t& aCardState) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyIccInfoChanged(const OptionalIccInfoData& aInfoData) MOZ_OVERRIDE;

private:
  // MOZ_FINAL suppresses -Werror,-Wdelete-non-virtual-dtor
  ~IccChild();

  void
  UpdateIccInfo(const OptionalIccInfoData& aInfoData);

  bool
  SendRequest(const IccRequest& aRequest, nsIIccCallback* aRequestReply);

  nsCOMArray<nsIIccListener> mListeners;
  nsRefPtr<IccInfo> mIccInfo;
  uint32_t mCardState;
  bool mIsAlive;
};

class IccRequestChild MOZ_FINAL : public PIccRequestChild
{
public:
  explicit IccRequestChild(nsIIccCallback* aRequestReply);

protected:
  virtual bool
  Recv__delete__(const IccReply& aReply) MOZ_OVERRIDE;

private:
  // MOZ_FINAL suppresses -Werror,-Wdelete-non-virtual-dtor
  virtual ~IccRequestChild() {
    MOZ_COUNT_DTOR(IccRequestChild);
  }

  nsCOMPtr<nsIIccCallback> mRequestReply;
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccChild_h
