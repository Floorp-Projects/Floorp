#ifndef mozilla_test_TestThreadChild_h
#define mozilla_test_TestThreadChild_h

#include "mozilla/ipc/GeckoThread.h"
#include "mozilla/test/TestChild.h"

namespace mozilla {
namespace test {

class TestThreadChild : public mozilla::ipc::GeckoThread
{
public:
    TestThreadChild();
    ~TestThreadChild();

protected:
    virtual void Init();
    virtual void CleanUp();

private:
    TestChild mChild;
};

} // namespace test
} // namespace mozilla

#endif // ifndef mozilla_test_TestThreadChild_h
