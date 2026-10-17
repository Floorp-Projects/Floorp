// SPDX-License-Identifier: MPL-2.0
#include "DisconnectCause.h"
#include <cassert>
using floorp::shim::DisconnectCause;
int main() {
  DisconnectCause cause;
  cause.Record("send-failed", 100, true);
  cause.Record("process-exited", 200, false);
  assert(cause.reason == "send-failed" && cause.wallTimeMs == 100 && cause.connected);
  cause.Record("service-stopped", 300, false);
  assert(cause.reason == "send-failed");
  cause.Reset();
  cause.Record("hello-timeout", 400, false);
  cause.Record("process-exited", 500, false);
  assert(cause.reason == "hello-timeout" && cause.wallTimeMs == 400 && !cause.connected);
  cause.Reset();
  cause.Record("process-exited", 600, false);
  assert(cause.reason == "process-exited" && cause.wallTimeMs == 600);
}
