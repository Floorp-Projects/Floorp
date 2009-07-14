#include "mozilla/test/TestParent.h"

using mozilla::test::TestParent;

void
TestParent::DoStuff()
{
    int ping;
    SendPing(&ping);

    printf("[TestParent] child replied to ping with status code %d\n", ping);
}

// C++ file contents
nsresult TestParent::RecvPing(int* status)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
