/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_backgroundchildimpl_h__
#define mozilla_ipc_backgroundchildimpl_h__

#include "mozilla/Attributes.h"
#include "mozilla/ipc/PBackgroundChild.h"

template <class> class nsAutoPtr;

namespace mozilla {
namespace dom {
namespace indexedDB {

class IDBTransaction;

} // namespace indexedDB
} // namespace dom

namespace ipc {

// Instances of this class should never be created directly. This class is meant
// to be inherited in BackgroundImpl.
class BackgroundChildImpl : public PBackgroundChild
{
public:
  class ThreadLocal;

  // Get the ThreadLocal for the current thread if
  // BackgroundChild::GetOrCreateForCurrentThread() has been called and true was
  // returned (e.g. a valid PBackgroundChild actor has been created or is in the
  // process of being created). Otherwise this function returns null.
  // This functions is implemented in BackgroundImpl.cpp.
  static ThreadLocal*
  GetThreadLocalForCurrentThread();

protected:
  BackgroundChildImpl();
  virtual ~BackgroundChildImpl();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PBackgroundTestChild*
  AllocPBackgroundTestChild(const nsCString& aTestArg) MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundTestChild(PBackgroundTestChild* aActor) MOZ_OVERRIDE;

  virtual PBackgroundIDBFactoryChild*
  AllocPBackgroundIDBFactoryChild(const OptionalWindowId& aOptionalWindowId)
                                  MOZ_OVERRIDE;

  virtual bool
  DeallocPBackgroundIDBFactoryChild(PBackgroundIDBFactoryChild* aActor)
                                    MOZ_OVERRIDE;

  virtual PBlobChild*
  AllocPBlobChild(const BlobConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPBlobChild(PBlobChild* aActor) MOZ_OVERRIDE;

  virtual PFileDescriptorSetChild*
  AllocPFileDescriptorSetChild(const FileDescriptor& aFileDescriptor)
                               MOZ_OVERRIDE;

  virtual bool
  DeallocPFileDescriptorSetChild(PFileDescriptorSetChild* aActor) MOZ_OVERRIDE;
};

class BackgroundChildImpl::ThreadLocal MOZ_FINAL
{
  friend class nsAutoPtr<ThreadLocal>;

public:
  mozilla::dom::indexedDB::IDBTransaction* mCurrentTransaction;

#ifdef MOZ_ENABLE_PROFILER_SPS
  uint64_t mNextTransactionSerialNumber;
  uint64_t mNextRequestSerialNumber;
#endif

public:
  ThreadLocal();

private:
  // Only destroyed by nsAutoPtr<ThreadLocal>.
  ~ThreadLocal();
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_backgroundchildimpl_h__
