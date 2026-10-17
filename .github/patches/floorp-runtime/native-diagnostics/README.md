# Native Shim diagnostic source patch

Apply `preserve-shim-disconnect-cause.patch` to Runtime7dd619742b25567e479eeda97ae071d10542ad0b in isolation. This patch is not in the package source-patch path and is not applied by existing-artifact CI. Native recompilation is required before it changes browser observations.

The existing terminal `reason` remains unchanged. Per launch, `causeReason`, `causeTimeMs`, `connectedBeforeDisconnect`, and PID preserve the first disconnect cause before native teardown; watchdog and cancellation paths are included. This does not identify silent Hello validation rejection. No signature/audit-token/Mach/nonce/ownership predicate, timeout, signal or publication gate is relaxed.

Compile the portable state regression against the patched source header:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -I <patched-runtime>/widget/cocoa/appshim test_disconnect_cause.cpp -o /tmp/test-disconnect-cause
/tmp/test-disconnect-cause
```

This checks the actual portable state helper, including overwrite prevention and relaunch reset. Cocoa integration is independently reviewed but is not compiled or executed by this Linux test or an existing-artifact diagnostic. The Python regression separately demonstrates the existing collector dropping cause/order fields. No native build is performed in this scope.
