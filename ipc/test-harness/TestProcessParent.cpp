#include "mozilla/test/TestProcessParent.h"
using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {
namespace test {


TestProcessParent::TestProcessParent() :
    GeckoChildProcessHost(GeckoChildProcess_TestHarness)
{
}

TestProcessParent::~TestProcessParent()
{
}


} // namespace test
} // namespace mozilla
