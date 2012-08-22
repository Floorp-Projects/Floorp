#include "TestSanity.h"

#include "IPDLUnitTests.h"      // fail etc.

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestSanityParent::TestSanityParent()
{
    MOZ_COUNT_CTOR(TestSanityParent);
}

TestSanityParent::~TestSanityParent()
{
    MOZ_COUNT_DTOR(TestSanityParent);
}

void
TestSanityParent::Main()
{
    if (!SendPing(0, 0.5f, 0))
        fail("sending Ping");
}


bool
TestSanityParent::RecvPong(const int& one, const float& zeroPtTwoFive,
                           const uint8_t&/*unused*/)
{
    if (1 != one)
        fail("invalid argument `%d', should have been `1'", one);

    if (0.25f != zeroPtTwoFive)
        fail("invalid argument `%g', should have been `0.25'", zeroPtTwoFive);

    Close();

    return true;
}


//-----------------------------------------------------------------------------
// child

TestSanityChild::TestSanityChild()
{
    MOZ_COUNT_CTOR(TestSanityChild);
}

TestSanityChild::~TestSanityChild()
{
    MOZ_COUNT_DTOR(TestSanityChild);
}

bool
TestSanityChild::RecvPing(const int& zero, const float& zeroPtFive,
                          const int8_t&/*unused*/)
{
    if (0 != zero)
        fail("invalid argument `%d', should have been `0'", zero);

    if (0.5f != zeroPtFive)
        fail("invalid argument `%g', should have been `0.5'", zeroPtFive);

    if (!SendPong(1, 0.25f, 0))
        fail("sending Pong");
    return true;
}


} // namespace _ipdltest
} // namespace mozilla
