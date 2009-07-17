#include "mozilla/test/TestParent.h"

using mozilla::test::TestParent;

void
TestParent::DoStuff()
{
    puts("[TestParent] pinging child ...");
    SendPing();
}

// C++ file contents
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

TestParent::TestParent()
{
}

TestParent::~TestParent()
{
}
