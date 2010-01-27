#include "TestBlockChild.h"

#include "IPDLUnitTests.h"      // fail etc.

template<>
struct RunnableMethodTraits<mozilla::_ipdltest::TestBlockChildChild>
{
    typedef mozilla::_ipdltest::TestBlockChildChild T;
    static void RetainCallee(T* obj) { }
    static void ReleaseCallee(T* obj) { }
};


namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

void
TestBlockChildParent::Main()
{
    BlockChildInLowerFrame();

    if (!SendPoke1())
        fail("couldn't poke the child");

    if (!CallStackFrame())
        fail("couldn't poke the child");

    if (!SendLastPoke())
        fail("couldn't poke the child");

    if (!UnblockChild())
        fail("can't UnblockChild()!");
    mChildBlocked = false;
}

void
TestBlockChildParent::BlockChildInLowerFrame()
{
    if (!BlockChild())
        fail("can't BlockChild()!");
    // child should be blocked and stay blocked after this returns
    mChildBlocked = true;
}

bool
TestBlockChildParent::AnswerStackFrame()
{
    if (!SendPoke2())
        fail("couldn't poke the child");
    return true;
}

bool
TestBlockChildParent::RecvP1()
{
    if (mChildBlocked)
        fail("child shouldn't been able to slip this past the blockade!");
    mGotP1 = true;
    return true;
}

bool
TestBlockChildParent::RecvP2()
{
    if (mChildBlocked)
        fail("child shouldn't been able to slip this past the blockade!");
    mGotP2 = true;
    return true;
}

bool
TestBlockChildParent::RecvDone()
{
    if (mChildBlocked)
        fail("child shouldn't been able to slip this past the blockade!");

    // XXX IPDL transition checking makes this redundant, but never
    // hurts to double-check, especially if we eventually turn off
    // state checking in OPT builds
    if (!mGotP1)
        fail("should have received P1!");
    if (!mGotP2)
        fail("should have received P2!");

    Close();

    return true;
}


//-----------------------------------------------------------------------------
// child

bool
TestBlockChildChild::RecvPoke1()
{
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &TestBlockChildChild::OnPoke1));
    return true;
}

bool
TestBlockChildChild::AnswerStackFrame()
{
    return CallStackFrame();
}

bool
TestBlockChildChild::RecvPoke2()
{
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &TestBlockChildChild::OnPoke2));
    return true;
}

bool
TestBlockChildChild::RecvLastPoke()
{
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &TestBlockChildChild::OnLastPoke));
    return true;
}

void
TestBlockChildChild::OnPoke1()
{
    if (!SendP1())
        fail("couldn't send P1");
}

void
TestBlockChildChild::OnPoke2()
{
    if (!SendP2())
        fail("couldn't send P2");
}

void
TestBlockChildChild::OnLastPoke()
{
    if (!SendDone())
        fail("couldn't send Done");
}

} // namespace _ipdltest
} // namespace mozilla
