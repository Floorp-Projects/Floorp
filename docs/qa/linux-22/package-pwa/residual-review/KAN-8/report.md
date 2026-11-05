# KAN-8: new complete-Flatpak residual measurements

The existing fixed PR198 complete package was reused; no package rebuild or
repeat of the old path-only smoke suite was performed. These new tests address
the two explicit residuals: actual cache-content reuse/migration and exceptional
registry handling through the installed launcher.

Subject: canonical Flathub PR198 source d9aa24d58fe633bf765721f95b4ac13132ee397e,
installed local OSTree commit
7a3b387dc2c79a14b8f49b603b5b8d85d720fb233b325238d9d4d7a03bafde5a,
Floorp12.21.1/native159.0.1/BuildID20261103153232. The installed real launcher and
helper match the recorded source SHA256 values and remain unchanged. This is
the existing locally built **unsigned** full package, not Flathub-signed release
qualification. No helper injection, command override or explicit --profile.

## HTTP cache contents, not only directory names

A dedicated loopback HTTP fixture serves a no-store HTML page and a stable,
cacheable65555byte payload. The real page fetches that same payload URL. The
HTTP server counts actual payload requests; the page records length, identifying
prefix and ResourceTiming transferSize. The browser is normally quit between
every row, so its in-memory cache cannot explain later hits.

| Actual installed product launch | Payload request counter | transferSize | Profile marker | Exit |
|---|---:|---:|---|---:|
| New legacy root, home permission absent |1 |65855 |retained |0 |
| One-run home permission enabled |1 |0 |retained |0 |
| Home permission removed |1 |0 |retained |0 |
| Existing XDG-only profile fixture |1 |0 |retained |0 |
| XDG-only with home permission enabled |1 |0 |retained |0 |

All5 payload responses retain the known65555byte length and identifying prefix.
The first response comes from the HTTP server and all4 subsequent cold-product
launches obtain the cached body without another payload request. Each normal
quit leaves a real cache2/entries file; ProfLD remains identical. This is bounded
real HTTP disk-cache-content reuse, not exhaustive arbitrary cache correctness
or bitwise validation of every cached resource type.

The XDG-only fixture is made by moving this freshly generated, normally closed
legacy registry/profile directory into config/floorp before launching the actual
default wrapper again. The wrapper selects the existing XDG profile, retains its
preference marker and reuses the same local cache. This is a controlled input
migration test, **not a claim that the helper automatically moves user data**.

The legacy-home-enabled.png screenshot was visually inspected: the fixture
displays length65555, expected prefix and transferSize0. The request counter and
profile/normal-quit JSON are authoritative evidence.

## Installed launcher mixed-registry guards

Three separate synthetic, non-user fixtures are passed to the actual installed
default Flatpak launcher, not to an extracted/sourced helper:

| Fixture | Expected installed-launcher outcome | Actual |
|---|---|---|
| Separate legacy and XDG minimal profiles.ini registries |Refuse ambiguous choice |exit1, explicit two-registry diagnostic |
| Separate host profile visible with home permission |Refuse separate host data |exit1, explicit host-profile diagnostic |
| Legacy data without profiles.ini |Refuse partial data |exit1, explicit missing-registry diagnostic |

All fixture file sets and SHA256 values are identical before/after each refusal.
These are intended fail-closed guard results, not browser crashes or reproduced
product defects. The minimal registries exercise launcher checks before Gecko
parses a real profile. Other unusual alias/hardlink/custom-override combinations
remain covered only by the previously recorded unit suite, not these3 actual
package guard fixtures.

## Environment and retained harness failure

Debian13.6, owned Xvfb104/X11, normal separate DBus session per browser run,
Flatpak1.16.6/runtime26.08. The separately owned system-test bus is deliberately
identified as **incomplete**: it has no full portal/system desktop services and
is not a Bazzite desktop. All5 browser runs use security.sandbox.content.level6,
absent sandbox-disable environment, disk-cache enabled and normal in-app
quit/forcedfalse/exit0. No signature/TLS/content protection was disabled.

The first HTTP attempt read a page-window expando through the Marionette content
sandbox and got null although navigation completed. It is retained under
attempt1-window-expando/ as an instrumentation failure, with normal product quit
and owned-process0. The final script reads the real page's rendered DOM JSON
instead; no product/security change. No product defect was found.

All owned processes are stopped and X104's socket is absent. Only launcher/helper
hashes were explicitly rehashed; the immutable installed OSTree commit identity
was also checked. Do not describe this limited hash check as a new whole-runtime
byte audit.

## Disposition and remaining original conditions

This removes the generic **untested cache contents** and **untested all mixed
registries in an actual package** gaps within the explicit cases above. It does
not remove the original **Bazzite44/Flatseal/Bazaar**, original permission desktop
integration, complete portal/system bus, or official Flathub build/signature
review conditions. Keep their needs/env labels when they specifically refer to
those original conditions. No new code defect, new fix PR, merge/release/Done or
external message was produced by this subtask; canonical PR198 remains the fix.

Replay: run residual_probe.py in a fresh evidence root on this QA machine with
the existing candidate installation. It requires isolated display104/port2843
and loopback18088. Validate existing records without relaunching with
`python3 validate.py result.json`.

Publish only report.md, matrix.json, result.json, scripts, execution/browser logs
and PNGs. Exclude all *-home/, runtime/, system-probe-bus sockets and their nested
profile/browser data, including in the retained first-attempt directory.
