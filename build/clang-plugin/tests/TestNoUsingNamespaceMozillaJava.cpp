namespace mozilla {
namespace java {
namespace sdk {
}  // namespace sdk

namespace future {
}  // namespace future
}  // namespace java
}  // namespace mozilla

namespace mozilla {
  using namespace java;  // expected-error{{using namespace mozilla::java is forbidden}}
  using namespace java::future;  // expected-error{{using namespace mozilla::java::future is forbidden}}
}

using namespace mozilla::java::sdk;  // expected-error{{using namespace mozilla::java::sdk is forbidden}}

namespace shouldPass {
  namespace java {
  }

  using namespace java;
}

using namespace shouldPass::java;


void test() {
}
