/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=4 ts=4 et :
 */
#include "TestDemon.h"

#include <stdlib.h>

#include "IPDLUnitTests.h"      // fail etc.
#if defined(OS_POSIX)
#include <sys/time.h>
#include <unistd.h>
#else
#include <time.h>
#include <windows.h>
#endif

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestDemonParent>
{
    static void RetainCallee(mozilla::_ipdltest::TestDemonParent* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestDemonParent* obj) { }
};

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestDemonChild>
{
    static void RetainCallee(mozilla::_ipdltest::TestDemonChild* obj) { }
    static void ReleaseCallee(mozilla::_ipdltest::TestDemonChild* obj) { }
};

namespace mozilla {
namespace _ipdltest {

const int kMaxStackHeight = 4;

static LazyLogModule sLogModule("demon");

#define DEMON_LOG(...) MOZ_LOG(sLogModule, LogLevel::Debug, (__VA_ARGS__))

static int gStackHeight = 0;
static bool gFlushStack = false;

static int
Choose(int count)
{
#if defined(OS_POSIX)
  return random() % count;
#else
  return rand() % count;
#endif
}

//-----------------------------------------------------------------------------
// parent

TestDemonParent::TestDemonParent()
 : mDone(false),
   mIncoming(),
   mOutgoing()
{
  MOZ_COUNT_CTOR(TestDemonParent);
}

TestDemonParent::~TestDemonParent()
{
  MOZ_COUNT_DTOR(TestDemonParent);
}

void
TestDemonParent::Main()
{
  if (!getenv("MOZ_TEST_IPC_DEMON")) {
    QuitParent();
    return;
  }
#if defined(OS_POSIX)
  srandom(time(nullptr));
#else
  srand(time(nullptr));
#endif

  DEMON_LOG("Start demon");

  if (!SendStart())
	fail("sending Start");

  RunUnlimitedSequence();
}

#ifdef DEBUG
bool
TestDemonParent::ShouldContinueFromReplyTimeout()
{
  return Choose(2) == 0;
}

bool
TestDemonParent::ArtificialTimeout()
{
  return Choose(5) == 0;
}

void
TestDemonParent::ArtificialSleep()
{
  if (Choose(2) == 0) {
    // Sleep for anywhere from 0 to 100 milliseconds.
    unsigned micros = Choose(100) * 1000;
#ifdef OS_POSIX
    usleep(micros);
#else
    Sleep(micros / 1000);
#endif
  }
}
#endif

bool
TestDemonParent::RecvAsyncMessage(const int& n)
{
  DEMON_LOG("Start RecvAsync [%d]", n);

  MOZ_ASSERT(n == mIncoming[0]);
  mIncoming[0]++;

  RunLimitedSequence();

  DEMON_LOG("End RecvAsync [%d]", n);
  return true;
}

bool
TestDemonParent::RecvHiPrioSyncMessage()
{
  DEMON_LOG("Start RecvHiPrioSyncMessage");
  RunLimitedSequence();
  DEMON_LOG("End RecvHiPrioSyncMessage");
  return true;
}

bool
TestDemonParent::RecvSyncMessage(const int& n)
{
  DEMON_LOG("Start RecvSync [%d]", n);

  MOZ_ASSERT(n == mIncoming[0]);
  mIncoming[0]++;

  RunLimitedSequence(ASYNC_ONLY);

  DEMON_LOG("End RecvSync [%d]", n);
  return true;
}

bool
TestDemonParent::RecvUrgentAsyncMessage(const int& n)
{
  DEMON_LOG("Start RecvUrgentAsyncMessage [%d]", n);

  MOZ_ASSERT(n == mIncoming[2]);
  mIncoming[2]++;

  RunLimitedSequence(ASYNC_ONLY);

  DEMON_LOG("End RecvUrgentAsyncMessage [%d]", n);
  return true;
}

bool
TestDemonParent::RecvUrgentSyncMessage(const int& n)
{
  DEMON_LOG("Start RecvUrgentSyncMessage [%d]", n);

  MOZ_ASSERT(n == mIncoming[2]);
  mIncoming[2]++;

  RunLimitedSequence(ASYNC_ONLY);

  DEMON_LOG("End RecvUrgentSyncMessage [%d]", n);
  return true;
}

void
TestDemonParent::RunUnlimitedSequence()
{
  if (mDone) {
    return;
  }

  gFlushStack = false;
  DoAction();

  MessageLoop::current()->PostTask(NewRunnableMethod(this, &TestDemonParent::RunUnlimitedSequence));
}

void
TestDemonParent::RunLimitedSequence(int flags)
{
  if (gStackHeight >= kMaxStackHeight) {
    return;
  }
  gStackHeight++;

  int count = Choose(20);
  for (int i = 0; i < count; i++) {
	if (!DoAction(flags)) {
      gFlushStack = true;
    }
    if (gFlushStack) {
      gStackHeight--;
      return;
    }
  }

  gStackHeight--;
}

static bool
AllowAsync(int outgoing, int incoming)
{
  return incoming >= outgoing - 5;
}

bool
TestDemonParent::DoAction(int flags)
{
  if (flags & ASYNC_ONLY) {
    if (AllowAsync(mOutgoing[0], mIncoming[0])) {
      DEMON_LOG("SendAsyncMessage [%d]", mOutgoing[0]);
      return SendAsyncMessage(mOutgoing[0]++);
    } else {
      return true;
    }
  } else {
	switch (Choose(3)) {
     case 0:
      if (AllowAsync(mOutgoing[0], mIncoming[0])) {
        DEMON_LOG("SendAsyncMessage [%d]", mOutgoing[0]);
        return SendAsyncMessage(mOutgoing[0]++);
      } else {
        return true;
      }

     case 1: {
       DEMON_LOG("Start SendHiPrioSyncMessage");
       bool r = SendHiPrioSyncMessage();
       DEMON_LOG("End SendHiPrioSyncMessage result=%d", r);
       return r;
     }

     case 2:
      DEMON_LOG("Cancel");
      GetIPCChannel()->CancelCurrentTransaction();
      return true;
	}
  }
  MOZ_CRASH();
  return false;
}

//-----------------------------------------------------------------------------
// child


TestDemonChild::TestDemonChild()
 : mIncoming(),
   mOutgoing()
{
  MOZ_COUNT_CTOR(TestDemonChild);
}

TestDemonChild::~TestDemonChild()
{
  MOZ_COUNT_DTOR(TestDemonChild);
}

bool
TestDemonChild::RecvStart()
{
#ifdef OS_POSIX
  srandom(time(nullptr));
#else
  srand(time(nullptr));
#endif

  DEMON_LOG("RecvStart");

  RunUnlimitedSequence();
  return true;
}

#ifdef DEBUG
void
TestDemonChild::ArtificialSleep()
{
  if (Choose(2) == 0) {
    // Sleep for anywhere from 0 to 100 milliseconds.
    unsigned micros = Choose(100) * 1000;
#ifdef OS_POSIX
    usleep(micros);
#else
    Sleep(micros / 1000);
#endif
  }
}
#endif

bool
TestDemonChild::RecvAsyncMessage(const int& n)
{
  DEMON_LOG("Start RecvAsyncMessage [%d]", n);

  MOZ_ASSERT(n == mIncoming[0]);
  mIncoming[0]++;

  RunLimitedSequence();

  DEMON_LOG("End RecvAsyncMessage [%d]", n);
  return true;
}

bool
TestDemonChild::RecvHiPrioSyncMessage()
{
  DEMON_LOG("Start RecvHiPrioSyncMessage");
  RunLimitedSequence();
  DEMON_LOG("End RecvHiPrioSyncMessage");
  return true;
}

void
TestDemonChild::RunUnlimitedSequence()
{
  gFlushStack = false;
  DoAction();

  MessageLoop::current()->PostTask(NewRunnableMethod(this, &TestDemonChild::RunUnlimitedSequence));
}

void
TestDemonChild::RunLimitedSequence()
{
  if (gStackHeight >= kMaxStackHeight) {
    return;
  }
  gStackHeight++;

  int count = Choose(20);
  for (int i = 0; i < count; i++) {
    if (!DoAction()) {
      gFlushStack = true;
    }
    if (gFlushStack) {
      gStackHeight--;
      return;
    }
  }

  gStackHeight--;
}

bool
TestDemonChild::DoAction()
{
  switch (Choose(6)) {
   case 0:
    if (AllowAsync(mOutgoing[0], mIncoming[0])) {
      DEMON_LOG("SendAsyncMessage [%d]", mOutgoing[0]);
      return SendAsyncMessage(mOutgoing[0]++);
    } else {
      return true;
    }

   case 1: {
     DEMON_LOG("Start SendHiPrioSyncMessage");
     bool r = SendHiPrioSyncMessage();
     DEMON_LOG("End SendHiPrioSyncMessage result=%d", r);
     return r;
   }

   case 2: {
     DEMON_LOG("Start SendSyncMessage [%d]", mOutgoing[0]);
     bool r = SendSyncMessage(mOutgoing[0]++);
     switch (GetIPCChannel()->LastSendError()) {
       case SyncSendError::PreviousTimeout:
       case SyncSendError::SendingCPOWWhileDispatchingSync:
       case SyncSendError::SendingCPOWWhileDispatchingUrgent:
       case SyncSendError::NotConnectedBeforeSend:
       case SyncSendError::CancelledBeforeSend:
         mOutgoing[0]--;
         break;
       default:
         break;
     }
     DEMON_LOG("End SendSyncMessage result=%d", r);
     return r;
   }

   case 3:
	DEMON_LOG("SendUrgentAsyncMessage [%d]", mOutgoing[2]);
	return SendUrgentAsyncMessage(mOutgoing[2]++);

   case 4: {
     DEMON_LOG("Start SendUrgentSyncMessage [%d]", mOutgoing[2]);
     bool r = SendUrgentSyncMessage(mOutgoing[2]++);
     switch (GetIPCChannel()->LastSendError()) {
       case SyncSendError::PreviousTimeout:
       case SyncSendError::SendingCPOWWhileDispatchingSync:
       case SyncSendError::SendingCPOWWhileDispatchingUrgent:
       case SyncSendError::NotConnectedBeforeSend:
       case SyncSendError::CancelledBeforeSend:
         mOutgoing[2]--;
         break;
       default:
         break;
     }
     DEMON_LOG("End SendUrgentSyncMessage result=%d", r);
     return r;
   }

   case 5:
	DEMON_LOG("Cancel");
	GetIPCChannel()->CancelCurrentTransaction();
	return true;
  }
  MOZ_CRASH();
  return false;
}

} // namespace _ipdltest
} // namespace mozilla
