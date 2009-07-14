#include "mozilla/test/TestProtocolChild.h"

namespace mozilla {
namespace test {

// Header file contents
class TestChild :
    public TestProtocolChild
{
protected:
    virtual nsresult RecvPing(int* status);
    virtual nsresult RecvTellValue(
                const String& key,
                const String& val);
    virtual nsresult RecvTellValues(
                const StringArray& keys,
                const StringArray& vals);

public:
    TestChild();
    virtual ~TestChild();
};

} // namespace test
} // namespace mozilla
