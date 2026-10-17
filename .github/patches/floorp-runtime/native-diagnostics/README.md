# Native Shim diagnostic source patch

Apply `preserve-shim-disconnect-cause.patch` to Runtime7dd619742b25567e479eeda97ae071d10542ad0b in isolation. This patch is not in the package source-patch path and is not applied by existing-artifact CI. Native recompilation is required before it changes browser observations.

The existing terminal `reason` remains unchanged. Per launch, `causeReason`, `causeTimeMs`, `connectedBeforeDisconnect`, and PID preserve the first disconnect cause before native teardown; watchdog and cancellation paths are included. This does not identify silent Hello validation rejection. No signature/audit-token/Mach/nonce/ownership predicate, timeout, signal or publication gate is relaxed.

Compile the portable state regression against the patched source header:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -I <patched-runtime>/widget/cocoa/appshim test_disconnect_cause.cpp -o /tmp/test-disconnect-cause
/tmp/test-disconnect-cause
```

This checks the actual portable state helper, including overwrite prevention and relaunch reset. Cocoa integration is independently reviewed but is not compiled or executed by this Linux test or an existing-artifact diagnostic. The Python regression separately demonstrates the existing collector dropping cause/order fields. These portable tests and the existing-artifact diagnostic do not perform native builds.

`cocoa_probe.py` provides a separate, limited native Foundation check. It pins
three Runtime7dd source files and the maintained patch by SHA-256, applies the
patch in an isolated temporary source tree, and extracts the exact changed
RememberCause method and disconnect payload expression. It compiles them and
the actual Protocol.mm codec with Objective-C++/ARC on a standard Mac Intel
runner, then checks first-cause/time/host-state preservation, NSString lifetime,
NSNumber/JSON round trips, reset, and codec rejection boundaries.

This probe does not compile the complete MacWebAppService translation unit or
link xul. It does not execute Disconnect, Stop, the watchdogs or authentication
and cannot establish the cause of either old failed run. Its SDK/compiler/flags
are recorded independently from the original build. Real host execution still
needs compatible generated headers/objects, a changed xul link and a new test
package. A small probe pass is neither compiled Debug nor product qualification.

The probe workflow has one ten-minute Mac Intel job, a single exact push branch,
read-only contents permission and no inherited secrets. It needs no browser,
signing credentials, cache, dependency installation or settings change. A branch
push is an execution request; record its distinct purpose before publishing.

The published execution route is the Runtime companion workflow maintained in
`../ci/mac-native-cocoa-probe.patch`, applied to Runtime7dd. It checks out the
immutable Floorp fixture `cbe45a7d7480d713b2c40a9ed348420126e6ccb0`.
The Runtime branch is `codex/mac-native-cocoa-probe-20261017`; the Floorp source
branch is `codex/mac-native-cocoa-probe-source-20261017` and starts no job.
The draft Floorp execution workflow is omitted from the published source tree.
Both branches preserve existing workflows, gates and authentication settings.
The single Runtime probe emits its bounded report to the job log and saves the
same report as a one-day artifact. No old failed run is rerun or replaced.
