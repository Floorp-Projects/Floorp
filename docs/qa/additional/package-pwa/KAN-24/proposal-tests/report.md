# KAN-24 canonical/proposed actual-browser API regression

Canonical PR2950 head69d306b11c9e52da9e6270870740d49b1d561659 fails all four newly tested conditions; the minimal two-file proposal passes all four. No canonical worktree, PR, or product files were modified.

| API condition | Canonical actual | Proposed actual | Expected |
| --- | --- | --- | --- |
| observer registration: native false, local style vertical | vertical, no apply | horizontal, one apply | horizontal |
| observer registration: native true, local style horizontal | horizontal, no apply | vertical, one apply | vertical |
| stale form horizontal; external pref writer sets multirow; unrelated favicon save | horizontal | multirow | multirow |
| stale form multirow; external pref writer sets horizontal; unrelated favicon save | multirow | horizontal | horizontal |

Both unrelated favicon edits really saved; native remained false. External pref writes deliberately model another window's newer persisted value. This is API-level evidence, not an actual two-window UI interaction. The observer local-style callback tests the real production helper against actual Services.prefs; it does not demonstrate full chrome config initialization by itself.

Exact canonical dataManager/observer source and dependencies were compiled with esbuild. The proposed bundle uses onLoad in-memory transformations equivalent to additional/KAN24/minimal-fix-proposal.patch; proposed source copies, source/bundle hashes and the construction script are retained. Real unmodified rpc.ts selected directServicesFunctions because the actual chrome location was chrome://browser/content/browser.xhtml (usesSettingsActor=false). No RPC/Services mocks, settings-pref stand-ins, deleted default prefs, signature changes or sandbox changes were used. Inline CSS is loaded as actual text and production DEV=false is a compile flag.

Executed inside actual immutable /workspace/qa-linux-full-build/products/3040-r2/floorp, native version159.0.1/BuildID20261103153232, sandbox6, no sandbox-disable environment. Dedicated Xvfb101/DBus/fresh profile/port2911; profile and executable guards passed. This is not a new KAN24 full package, Wayland compositor evidence or product UI proof. Only source API bundles were injected with standard privileged Marionette test execution. Three modified preferences (design JSON, native vertical-tabs bool, split-view drag bool) and observer registrations are restored in each case's finally block.

Initial harness attempt used window.eval and was correctly blocked by CSP before any case ran. It is preserved in attempt1-csp-result.json as an instrumentation failure; no protection was relaxed. The successful run uses standard Marionette script evaluation. The initial failed attempt needed owned-process termination; the successful run used normal unforced in-app Quit with browser exit0. All owned processes stopped and all product file hashes were unchanged.

Primary evidence: result.json, matrix.json, cases.js, build.cjs, run.py, build-provenance.json, canonical.js, proposed.js, proposed source copies and browser.log. Browser logs can contain unrelated product startup warnings; test outcomes come from explicit assertions/results above. Missing-native-pref behavior on Gecko159 remains code-only; no default preference was deleted to manufacture that condition.

## Concrete proposal package and validation

The two-file proposal remains at ../additional/KAN24/minimal-fix-proposal.patch. patch-apply-checks.json verifies it applies to exact canonical head; the canonical eight-file delta applies to scoped latest-main29a3 files, and then the amendment applies. The resulting two source files match the API-tested in-memory proposed source bytes exactly. This is patch applicability and API validation, not a full integrated candidate build or standard repository test-suite pass.

Reproduce in this environment with `bash /workspace/qa-linux-additional/KAN-24/proposal-tests/reproduce.sh`. It creates a fresh evidence/profile directory, uses dedicated display101/port2911 (must be free), runs both exact bundles in actual unchanged Floorp, and validates the four differential results, sandbox, normal Quit and product invariance. To validate saved evidence alone: `python3 validate-result.py result.json`. Inventory snapshot: product-hashes-before.json; result.json confirms equality after testing. No owned processes remain.

Before integrating into canonical2950, update the inconsistent preexisting observer fixtures described in ../code-review.md and add these four cases to appropriate repository browser tests. No untested fixture-change or repository-test integration patch is supplied as passed. The provided four-case actual-source API runner is the tested regression artifact. Full chrome startup, Hub event handling/two actual windows, sidebar.revamp native invariants and workspace layout remain separately scoped UI validation.
