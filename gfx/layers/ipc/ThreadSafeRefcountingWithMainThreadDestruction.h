/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef THREADSAFEREFCOUNTINGWITHMAINTHREADDESTRUCTION_H_
#define THREADSAFEREFCOUNTINGWITHMAINTHREADDESTRUCTION_H_

#include "MainThreadUtils.h"
#include "base/message_loop.h"
#include "base/task.h"

namespace mozilla {
namespace layers {

inline MessageLoop* GetMainLoopAssertingMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  return MessageLoop::current();
}

inline MessageLoop* GetMainLoop()
{
  static MessageLoop* sMainLoop = GetMainLoopAssertingMainThread();
  return sMainLoop;
}

struct HelperForMainThreadDestruction
{
  HelperForMainThreadDestruction()
  {
    MOZ_ASSERT(NS_IsMainThread());
    GetMainLoop();
  }

  ~HelperForMainThreadDestruction()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }
};

} // namespace layers
} // namespace mozilla

#define NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(_class) \
public:                                                                       \
  NS_METHOD_(MozExternalRefCountType) AddRef(void) {                          \
    MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(_class)                                \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    nsrefcnt count = ++mRefCnt;                                               \
    NS_LOG_ADDREF(this, count, #_class, sizeof(*this));                       \
    return (nsrefcnt) count;                                                  \
  }                                                                           \
  static void DestroyToBeCalledOnMainThread(_class* ptr) {                    \
    MOZ_ASSERT(NS_IsMainThread());                                            \
    NS_LOG_RELEASE(ptr, 0, #_class);                                          \
    delete ptr;                                                               \
  }                                                                           \
  NS_METHOD_(MozExternalRefCountType) Release(void) {                         \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                          \
    nsrefcnt count = --mRefCnt;                                               \
    if (count == 0) {                                                         \
      if (NS_IsMainThread()) {                                                \
        NS_LOG_RELEASE(this, 0, #_class);                                     \
        delete this;                                                          \
      } else {                                                                \
        /* no NS_LOG_RELEASE here, will be in the runnable */                 \
        MessageLoop *l = ::mozilla::layers::GetMainLoop();                    \
        l->PostTask(FROM_HERE,                                                \
                    NewRunnableFunction(&DestroyToBeCalledOnMainThread,       \
                                        this));                               \
      }                                                                       \
    } else {                                                                  \
      NS_LOG_RELEASE(this, count, #_class);                                   \
    }                                                                         \
    return count;                                                             \
  }                                                                           \
protected:                                                                    \
  ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                                    \
private:                                                                      \
  ::mozilla::layers::HelperForMainThreadDestruction mHelperForMainThreadDestruction; \
public:

#endif