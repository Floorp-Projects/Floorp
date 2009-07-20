#include "mozilla/test/TestParent.h"

using mozilla::test::TestParent;

// C++ file contents
TestParent::TestParent()
{
}

TestParent::~TestParent()
{
}


void
TestParent::DoStuff()
{
#if 1
    puts("[TestParent] in DoStuff()");
    SendHello();
#elif 0
    puts("[TestParent] pinging child ...");
    SendPing();
#endif
}


#if 1
//-----------------------------------------------------------------------------
// "Hello world" exampl
nsresult TestParent::RecvWorld()
{
    puts("[TestParent] world!");
    return NS_OK;
}


#elif 0
//-----------------------------------------------------------------------------
// Example solution to exercise
nsresult TestParent::RecvPing()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TestParent::RecvPong(const int& status)
{
    printf("[TestParent] child replied to ping with status code %d\n", status);
    return NS_OK;
}

nsresult TestParent::RecvGetValue(const String& key)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TestParent::RecvGetValues(const StringArray& keys)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TestParent::RecvSetValue(
            const String& key,
            const String& val,
            bool* ok)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#endif
