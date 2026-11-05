# Actual workflow-package PWA test runner

`full_package_pwa_probe.py` is prepared and syntax/CLI checked. It must be run
only after successful exact-run artifact intake. No product test has been
performed by merely preparing this runner.

It consumes the package agent's `product-intake.json` and the original complete
`product-inventory-before.json`. It checks expected source SHA, successful run,
artifact ownership/digest/nonexpiry/name and complete extracted file inventory,
then runs the unmodified product at `productRoot`. It never substitutes an
omni.ja/manifest, never copies an old profile/SSB store, and verifies complete
product inventory equality again after execution.

Each run refuses an existing output root and creates a new HOME, browser profile
and test state. Only the automation port is written to user.js. It keeps content
sandbox level 6 and the observed disabled Linux experiment condition; a changed
condition fails qualification rather than forcing a preference or enrollment.
Input is synthetic UI element clicks and generated-launcher CLI startup.

1. Actual full package installs Alpha through its URL-bar UI and Beta through
   its menu, generating that package's own desktop entries, icons and SSB store.
2. The product normally quits. A cold process starts Alpha using its exact
   generated Exec, Beta's exact Exec starts a second PWA in that process, and
   an ordinary browser window follows. The runner verifies launcher executable,
   profile, app id and class provenance before executing arguments directly.
3. X11 mode records own-PID WM_CLASS. Wayland mode records actual protocol
   set_app_id plus own-PID KWin resourceClass/desktopFileName. It does not claim
   authenticated icon pixels or working Plasma taskbar behavior.
4. After another normal quit, actual packaged helper/window APIs test the nine
   old/malformed-record cases with a 120-second automation deadline. The
   existing wholly empty-record random fallback is an observation, not repaired
   or required to equal an independently generated random token.
5. Owned browser and display/compositor processes terminate; original product
   files must remain identical. Results distinguish execution completion from
   whether observed identities satisfy the intended product behavior, so a
   baseline class failure remains visible rather than being relabelled a pass.

## PR3032 full package

After the package agent authenticates run42068055038's artifact:

```sh
dbus-run-session -- python3 full_package_pwa_probe.py \
  --intake /workspace/qa-linux-22/package-pwa/full-build/3032/product-intake.json \
  --expected-sha 8249e234adfbb3e877e4a20cf5f57d7264d9a447 \
  --expected-run 42068055038 \
  --out /workspace/qa-linux-22/package-pwa/full-build/3032/x11-product-qa \
  --mode x11 --display :95 --port 2873 --fixture-port 8783
```

Then the equivalent separate native Wayland run, after X11 releases display95:

```sh
dbus-run-session -- python3 full_package_pwa_probe.py \
  --intake /workspace/qa-linux-22/package-pwa/full-build/3032/product-intake.json \
  --expected-sha 8249e234adfbb3e877e4a20cf5f57d7264d9a447 \
  --expected-run 42068055038 \
  --out /workspace/qa-linux-22/package-pwa/full-build/3032/wayland-product-qa \
  --mode wayland --nested-wayland --display :95 --port 2874 --fixture-port 8784
```

The Wayland run creates an owned nested KWin on an owned Xvfb in a dedicated
normal D-Bus session. No stale `/proc/18517` bus wrapper is used. It does not
start Plasma or relax compositor interface/permission filtering. Installed
sysroot libraries and Qt plugins are selected per process; no shared desktop
configuration is overwritten. Display94 is reserved for the separate branding
agent. Use the execution environment's approved tool path for UNIX sockets and
namespace support; do not disable the product sandbox to work around a harness
restriction.

## Baseline

Apply the same commands to the separately authenticated baseline run42068377281
at source984509d2053be7bae9027133bda7259cd706b208, replacing intake/out paths and
using ports2875/2876 and fixture ports8785/8786. Preserve the two source-specific
result trees. Root handles baseline artifact authentication/extraction. Root and
the branding agent own the separate ja/es-CL full-package comparison; this runner
does not duplicate or relabel its earlier manually substituted branding tests.
