# KAN-96: actual Troubleshoot Mode with controlled COOP/COEP

A new dedicated profile was launched with the product's normal `--safe-mode` command. Live `Services.appinfo.inSafeMode` was **true**, native159.0.1/BuildID20261103153232 and sandbox6. This is an actual safe-mode measurement, not a spoofed mode flag or changed security preference.

The authenticated complete main29a3eb23145da90706d8a94441373278521cc737 product was reused (package42093687541/artifact13446089131). No product resources, Runtime lock, experiment assignment, sandbox/CSP settings or protection checks were edited. Debian13.6, normal isolatedDBus, ownedXvfb103/X11, dedicated profile; synthetic realWebDriver button input, not physical input/GPU/KDE qualification.

## Measured result

- The actual startup screenshot and X11 window tree show a normal browser and no visible Troubleshoot modal. Therefore no dialog button, Refresh/Reset action or blind key was sent. `dialog-command.json` explicitly records the observed no-dialog decision. Safe mode was independently asserted from the actual application afterward.
- A controlled loopback page sends COOP:same-origin and COEP:require-corp. Actual content reports crossOriginIsolated=true, isSecureContext=true, SharedArrayBuffer=function; actual remoteType is webCOOP+COEP=http://127.0.0.1.
- NRProgressiveWebApp actor is available in that actual process while inSafeMode=true. The actual packaged URL-bar install button/panel button was clicked and a real `QA COOP Default` record was saved. The former actor/remote-type error is not reproduced in this safe-mode condition.
- The initial three-second snapshot caught an unfinished second window and is retained as an early observation, not a product failure. A separate **new cold safe-mode relaunch**, without repeating installation, then exercised the actual generated desktop command. The desktop file was unchanged; only explicit test arguments `--safe-mode --marionette` were appended and recorded. Within5.01seconds the app had ready browser startup, actual fixtureURL, taskbartab/floorpSsbWindow markers, webCOOP+COEP process, actor=true and crossOriginIsolated=true while appinfo.inSafeMode=true. See cold-result.json/cold-ready.json/cold-content.json and cold_relaunch.py. This now qualifies controlled-fixture safe-mode PWA cold readiness, independently of normal-mode results.

`startup-screen.png` and `after-install.png` were visually inspected. The latter shows the real fixture and its isolation diagnostic. JSON records appinfo.inSafeMode, process type, actor, stored app and the unfinished second-window state.

## Remaining

Original CachyOS/Arch package and full desktop/permissions, original external YouTube Music while in safe mode, real external cross-site/embedded-resource policies, genuine assigned-container enrollment, and physical input remain unqualified. This narrows a generic 'safe mode unavailable' condition to those concrete remaining cases; it does not remove the original reported environment. No new defect or speculative patch/PR is justified by this result.

Two normal Marionette Quits (initial creation and separate cold relaunch) returned exit0/forcedfalse/in_app=true; all product hashes unchanged, owned processes0, owned display socket absent. No normal-mode install was repeated. No Jira/sharedmatrix/source/PR changes by this subtask. OriginalLinux22 denominator is unchanged.

Replay `dbus-run-session -- python3 safe-mode/probe.py` in a fresh evidence-layout copy with the parent `probe.py` (the trusted COOP/COEP fixture class is extracted without running that parent runner). Inspect the startup screenshot before writing the explicit `dialog-command.json`; never select a Refresh/Reset button. Exclude run/profile, run/home and run/runtime from publication.

## One actual original-site attempt

A subsequent one-time real YouTube Music safe-mode navigation was network-blocked: host CONNECT403 beforeTLS and actual browser proxy-refusal error page, with strict TLS and normal managedproxy. See [original-site report](music/report.md). This is concrete remaining capability evidence, not product failure or successful site non-reproduction. No retry/alternatehost/auth/consent bypass.
