# KAN-24 complete-product comparison

All six requested sequential actual-product executions completed: baseline,
candidate, reverse baseline, reverse candidate, shipped-API baseline and
shipped-API candidate. The two startup mismatch directions were naturally
created through visible Hub and Firefox Settings controls, never preference
seeded. Baseline mismatches survive normal restart. The integrated complete
product reconciles both before Hub is loaded, and both live synchronization
directions, new windows, Workspace switches and multirow persistence pass.

The stale-form contract is now verified against the product's own emitted
settings ESM: **baseline fails both cases; integrated product passes both**.
This improves on the previous compiled-source API experiment; no replacement
implementation is injected into either product.

## Product origins

- Baseline main29a3eb23145da90706d8a94441373278521cc737, complete unsigned package
  run42093687541, artifact13446089131, archive ZIP
  SHA25605164de082b5b6c1f18ec70155e6ae4eebec14486cb983b1a013660fbf00239d.
- Integration493d2974e00e6902d86585d329eea1f9040ff791, main159 plus canonical2950
  and dependent3046 amendment, complete unsigned package run42094061748,
  artifact13447726956, archive ZIP
  SHA256210665cbe6a8b6e8d25dcaf3906d77b6d6e12066fd708631b7ef7cd4b40d8a26.

Authenticated intake metadata is in ../intake/ and
../full-product-intake-summary.json. These are complete products from the
existing application/package pipeline with the locked prebuilt native Runtime,
not generated-manifest/resource substitutions and not fresh Gecko compilation
or strict release qualification. Canonical2950's historical Runtime158 suite
is separate; these executions use live Runtime159.0.1 / BuildID20261103153232.

Live identity, executable/profile and sandbox6 were checked from Services.
Package application.ini/platform.ini metadata is recorded separately under
each product execution. Their packaging BuildIDs differ from the live native
Runtime BuildID; the files were not altered or treated as native identity.
The initially launched baseline process loaded the runner before the explicit
version assertion was added; its saved startup snapshots were independently
validated for the same exact version/BuildID/sandbox immediately afterward.

## Measured comparison

| Condition | Baseline complete product | Integration complete product |
|---|---|---|
| Cold nativefalse/storedvertical | Reproduced, survives normal restart | false/horizontal before Hub navigation |
| Cold nativetrue/storedhorizontal | Reproduced, survives normal restart | true/vertical before Hub navigation |
| Hub Vertical → native and rendered layout | Pass | Pass in both migrated profiles |
| Native Horizontal → stored style and Hub | Stale vertical reproduced | Horizontal synchronized |
| Native Vertical → stored style and Hub | Stale horizontal reproduced | Vertical synchronized |
| Real new window, settled startup | Not repeated in these new baseline runs | Both windows actually vertical in both profiles |
| Real Workspace creation, first→second→first | Not repeated in these new baseline runs | Multirow preserved in both profiles |
| Normal restart after multirow | Not repeated in these new baseline runs | Multirow preserved in both profiles |
| Shipped API: stale horizontal, latest multirow, unrelated favicon save | Incorrectly writes horizontal | Preserves latest multirow and favicon change |
| Shipped API: stale multirow, latest horizontal, unrelated favicon save | Incorrectly writes multirow | Preserves latest horizontal and favicon change |

The startup JSON records every tab URL in every browser window. Both baseline
restart and candidate migration snapshots contain only about:blank tabs, so
these new observations exclude a restored background Hub. The candidate
corrects persisted style at cold startup rather than merely computing a
different getter result after loading Hub.

Each candidate execution independently passes all ten expected outcome checks.
Each baseline execution passes all six expected reproduction/control checks.
Those check counts include instrumentation/provenance assertions and should
not be misreported as independent issue counts. Detailed machine-readable
conditions, source SHAs and per-run results are in matrix.json.

## Shipped-module API evidence

../ui/shipped_api.py reads the actual omni.ja dataManager asset and conservatively
proves unique getter/save exports and their design-settings semantics. It
dynamically imports that exact chrome://noraneko-settings/content/assets/ URI
from real about:hub content; existing module security and RPC remain intact.
No local compiled module, mocked Services, mocked RPC or product modification
is used. The stale object comes from the actual getter; a real external
Services.prefs writer models the latest style from another window. The actual
shipped save then changes faviconColor with hasTabStyleChanged=false.

Both baseline failures persist the favicon edit and nativefalse while reverting
the newer style. Both integrated-product cases preserve the newer style, the
favicon edit and nativefalse. Original design/native values are restored at the
end. This remains an API contract comparison, not proof of the timing of two
simultaneously visible Hub forms' autosave race.

## Environment, visual review and retained failures

Debian / owned Xvfb102 X11 / normal DBus / real synthetic WebDriver ElementClick
and native shadow buttons. This is not physical input, KDE, Wayland, GPU or
original macOS qualification. Screenshots were visually inspected:
baseline/03-hub-to-native-hub.png, baseline-reverse/05-hub-after-on.png,
candidate/08-new-window-ready.png and candidate-reverse/02-migration-hub.png.
The reverse migration screenshot clearly shows Vertical selected; the settled
new-window screenshot visibly shows the actual vertical tab strip. Some older
content screenshots show Photon above the fold, so their radio-state JSON is
the authoritative state rather than an off-screen visual claim. Normal UI
scrollIntoView was added to improve later captures, without CSS mutation or
rerunning already completed tests for images alone.

The first attempt's content about:blank Location.href assignment was correctly
denied when targeting privileged about:hub. baseline-attempt1-navigation/
preserves that harness failure. It quit normally with product files unchanged.
The runner was corrected to use standard WebDriver:Navigate; no security policy
or product protection changed. This failure is not a product bug or a failed
reproduction. No other actual execution failed.

Ten successful normal quits across the six completed executions returned exit0,
forced=false and in_app=true. Each complete product's entire file inventory is
unchanged; owned processes remaining0 and Xvfb102 lock/socket absent are recorded
in cleanup.json. The retained first attempt also quit normally.

## Remaining conditions and scope

Original macOS Photon remains outstanding and was not accessed while its owner
handles KAN10. The actual two-visible-window pending-save timing and overflowing
multirow geometry remain unqualified; only few-tab persistence was exercised.
No source correction was needed after these product results. The original
Linux22 denominator and seven-issue matrix remain unchanged; no GPU condition
is reclassified. No merge, release, distribution change or Done transition was
performed by these executions.

Reproduce with ../ui/README.md and validate recorded results with
`python3 ../ui/summarize.py`. Never publish profile/, migration-seed/, home/ or
runtime/ directories; retain only the reviewed small scripts, snapshots, logs,
screenshots and matrices.
