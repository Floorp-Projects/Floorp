#include "mozilla/test/TestThreadChild.h"
using mozilla::test::TestThreadChild;

using mozilla::ipc::GeckoThread;

TestThreadChild::TestThreadChild() :
    GeckoThread()
{
}

TestThreadChild::~TestThreadChild()
{
}

void
TestThreadChild::Init()
{
    GeckoThread::Init();
    mChild.Open(channel(), owner_loop());
}

void
TestThreadChild::CleanUp()
{
    GeckoThread::CleanUp();
    mChild.Close();
}
