#include "mozilla/test/TestProtocolChild.h"

namespace mozilla {
namespace test {

// Header file contents
class TestChild :
    public TestProtocolChild
{
protected:

#if 1
//-----------------------------------------------------------------------------
// "Hello world" example
    virtual nsresult RecvHello();


#elif 0
//-----------------------------------------------------------------------------
// Example solution to exercise
    virtual nsresult RecvPing();
    virtual nsresult RecvPong(const int& status);
    virtual nsresult RecvTellValue(
                const String& key,
                const String& val);
    virtual nsresult RecvTellValues(
                const StringArray& keys,
                const StringArray& vals);
#endif

public:
    TestChild();
    virtual ~TestChild();
};

} // namespace test
} // namespace mozilla
