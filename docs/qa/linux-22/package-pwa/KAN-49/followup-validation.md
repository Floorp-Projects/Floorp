# PR #3032 follow-up: early import, malformed records, native Wayland

Final code commit: `8249e234adfbb3e877e4a20cf5f57d7264d9a447`, based on the
initial class correction `06aaf5893645d2a33106e7657d0e0a58aa6029e0` and Floorp
main `984509d2053be7bae9027133bda7259cd706b208`.

## Cold Linux command-line startup / early import

The previous measured three-order candidate test already starts a fresh owned
process for each row, uses the actual generated desktop Exec plus the normal
Marionette automation flag, verifies exact ProfD and own PID, and normally quits
before the next row. The Alpha-first and Beta-first rows exercise
SSBCommandLineHandler at cold process startup with no existing browser window;
the browser-first row exercises later PWA windows in an existing process.
Thus `pwa-order-candidate/result.json` provides actual cold command-line evidence
for the new LinuxSupport import and class derivation. All three rows retained
sandbox 6 / rollout false / normal in-app Quit. These results are the initial
class correction; the final guard additionally changes only non-string candidate
handling, leaving the valid UUID path identical.

## Actual baseline / candidate malformed-record API calls

`pwa_legacy_probe.py` invoked the actual packaged `LinuxSupport.getPathInfo` and
`SsbRunnerUtils.openSsbWindow` methods in a dedicated profile (port 2842). It
created and closed its own about:blank windows, with no seeded stores, external
sites, experiment overrides, or weakened protection. Each completed product run
retained sandbox 6 / rollout false and ended with normal in-app Quit.

| Record condition | Baseline launcher helper | Initial class candidate | Final guard candidate |
| --- | --- | --- | --- |
| Valid braced UUID | UUID class | window matches helper | unchanged |
| Missing id, usable name | name fallback | opens, matches name fallback | unchanged |
| Brace-only id `{}`, usable name | name fallback | opens, matches fallback | unchanged |
| Punctuation-only id, usable name | name fallback | opens, matches fallback | unchanged |
| Uppercase braced id | lowercased id class | opens, matches fallback | unchanged |
| Missing id and name, usable URL | URL fallback | opens, matches URL fallback | unchanged |
| Numeric id `123`, usable name | helper throws; existing window API still opens | **new window API TypeError** | opens with name fallback |
| Object id, usable name | helper throws; existing window API still opens | **new window API TypeError** | opens with name fallback |
| No normalizable strings | random `floorp-<12 hex>` fallback | opens with random fallback | unchanged existing limit |

The extra early helper call exposed an existing normalizer assumption and moved
its exception into window creation. That was a real newly introduced regression,
not an environment failure. Commit `8249e234` guards candidates with
`typeof candidate === "string"` before the existing normalization. It skips
unusable types and follows the existing name/URL/random fallback order. The
additional colocated test covers missing, brace-only, numeric and object ids by
opening actual windows and checking their independently expected name classes.
The correction does not change stored ids or broaden command-line lookup:
`getSsbById` still requires strict equality to the string CLI argument.

No-normalizable-fields entries remain an existing limitation: separate helper
calls generate independent random fallback tokens, so launcher/window token
equality is not guaranteed for a wholly invalid record. This was not expanded
into a migration or identifier-repair project.

Evidence: `legacy-baseline.json`, `legacy-candidate.json` (initial regression),
`legacy-guard-candidate.json` (all nine window API calls open), and the actual
browser logs. A first final-guard batch hit the test harness's 30-second script
deadline; `legacy-guard-candidate-timeout.json` retains that setup failure. The
same batch completed under an explicit 120-second test deadline without changing
product preferences. It is not counted as a product timeout or test pass.

## Final regression execution and checks

`regression-guard-candidate.json` records both exact new colocated test bodies
passing inside the final isolated packaged candidate, with sandbox 6 and normal
Quit. `pwa-class-guard-typecheck.log` records scoped typecheck success; scoped
lint, format and git diff checks also passed before committing.

The candidate remains a disposable official-product copy with only two JavaScript
resources changed in omni.ja; this follow-up is not a full source build or signed
release qualification. Updated resource/file hashes are in
`pwa-class-candidate-provenance.json`. Root independently owns the standard CI
runner/artifact evidence and updates PR #3032.

## Native Wayland / desktop correspondence

`candidate-kde/ui-create/result.json` records successful actual URL-bar Alpha and
application-menu Beta creation using the final candidate on native Wayland in
the existing nested KWin 6.3.6 / Plasma session. It uses its own
`candidate-kde/profile` and port 2844, separate from the official KAN-48 product
profile/port 2855. The separate official product is never controlled by this
probe.

`candidate-kde/product-snapshot.json`, `kwin-windows.json`,
`desktop-correspondence.json` and `app-id-protocol.txt` independently agree:
the two PWA windows in one process have distinct `floorp-<UUID>` windowclass,
Wayland `xdg_toplevel.set_app_id`, KWin resourceClass/desktopFileName, and generated
desktop StartupWMClass. The ordinary browser keeps `floorp`. Sandbox level 6
and the false Linux taskbar experiment remain unchanged.

This validates Wayland identity propagation; it does not authenticate KWin icon
pixels or recover the remaining Plasma task-model issue described under KAN-48.
No claim is made that this fixes the distinct original correct-app_id/wrong-icon
report. The follow-up cold launcher result is recorded separately in
`candidate-kde/cold-launch-result.json`.

That final cold test completed successfully: a fresh candidate process started
Alpha through its real generated launcher, then accepted Beta's real launcher
into the same dedicated profile. Both windows loaded the fixture, advertised
their distinct matching classes, and KWin resourceClass/desktopFileName matched
the launcher identities. `candidate-kde/cold-app-id-protocol.txt` retains the
actual native Wayland set_app_id calls. The final process normally quit
(`forced:false`); sandbox 6 and rollout false were unchanged. All own native,
legacy and candidate-KDE profile browser processes were absent after cleanup,
as recorded in `pwa-owned-process-cleanup.json`. The separate official KAN-48
product and KWin/compositor were left running for root's final observation.
