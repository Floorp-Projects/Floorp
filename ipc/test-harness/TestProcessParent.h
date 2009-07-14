#ifndef mozilla_test_TestProcessParent_h
#define mozilla_test_TestProcessParent_h 1

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace test {
//-----------------------------------------------------------------------------

class TestProcessParent : public mozilla::ipc::GeckoChildProcessHost
{
public:
    TestProcessParent();
    ~TestProcessParent();

    /**
     * Asynchronously launch the plugin process.
     */
    // Could override parent Launch, but don't need to here
    //bool Launch();

private:
    DISALLOW_EVIL_CONSTRUCTORS(TestProcessParent);
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef mozilla_test_TestProcessParent_h
