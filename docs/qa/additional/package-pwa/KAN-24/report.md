> Latest continuation: dependent Draft3046, standard158/159 CI and six complete-product trials are complete. Both cold-start mismatch directions and shipped stale-save2cases are fixed in the full159 integration. See [current report](followup/report.md). The scoped-copy limits below describe the earlier investigation, not the newer complete-product results. Original7matrix/Linux22 unchanged.

# Additional KAN-24: Hub / native vertical-tabs synchronization

KAN-24 / GitHub2949 is an **additional target**. The original Linux22 denominator and package-pwa seven-issue matrix are unchanged. KAN-7 GPU-specific conditions were not exercised or reclassified by these software-display/UI tests.

## Coordination and canonical work

Read all3Jira comments (10063,10148,10256; paginated result isLast=true), the original issue and2GitHub comments, latest canonical Draft2950 metadata, all reviews/inline/conversation comments, and bounded parent coordination. Draft2950 remains open/Draft at69d306b11c9e52da9e6270870740d49b1d561659 in Walkmana-25/Floorp; its known initial-sync review4346185151 was already pending. The earlier Windows test10256 initialized preferences directly and did not exercise Hub rendering/restarts; it was not mistaken for the new GUI evidence. Parent recent progress concerns other targets; no alternate KAN24 candidate was identified in these checked sources. No other chat message, duplicate PR, existing PR edit or merge was sent.

Latest main29a3eb23145da90706d8a94441373278521cc737 and exact canonical2950 were checked out independently. A third branch/worktree applied only canonical2950's8-file delta cleanly to main; all8resulting target files exactly match the canonical head. Source/Runtime lock hashes and the original matrix fingerprint are in scope.json and candidate-build/*.json.

## Actual product result

On Debian13.6 / Xvfb X11 / normal DBus, the authenticated unmodified complete product reproduces the setting mismatch through real synthetic WebDriver UI actions: Hub Photon→Vertical sets native true and actual vertical layout; Firefox Settings' visible shadow Horizontal button turns the actual tab strip horizontal/native false, but the persisted Floorp style and reopened Hub radio remain vertical. The mismatch survives normal restart, settled new windows and real Workspace creation/switching. **The original reported persistence of a visibly vertical native tab strip was not reproduced**: actual orientation is horizontal. Original macOS26 remains unqualified.

The existing2950 generated-resource candidate corrects the live Hub/native roundtrip, preserves multirow through Workspace switches and normal restart, and gives settled new windows matching actual vertical geometry. Its initialization still leaves nativefalse/storedvertical in a naturally mismatched profile cloned after the baseline's real UI actions and normal shutdown. The candidate Hub getter reads horizontal while persisted style remains vertical. Session restore had already restored Hub on these startup snapshots; they are not a claim that no Hub had loaded.

UI comparison covers6condition rows in ui-matrix.json. The baseline native-on direction was not separately repeated; the candidate real native-on action is recorded. Few-tab multirow setting persistence was checked, not overflowing multirow geometry. The actual two-visible-window pending-save race was not established; current Hub autosaves and has no Apply button. Read ui-report.md for operation-level evidence, readiness, snapshots, retained harness failures and cleanup. Four successful normal-quit records and zero owned remaining processes are recorded; product files remain unchanged.

## Concrete amendment for existing2950

No duplicate implementation PR was created. The small artifact additional/KAN24/minimal-fix-proposal.patch:

1. Reconciles the current native preference synchronously after registering the observer.
2. For an unrelated save, derives the effective style from the **latest persisted style**, rather than the stale form's horizontal/multirow value. Explicit user style changes remain intentional writes.

Canonical code fails4new actual-source API conditions; the proposed two-file source passes4/4 in actual Gecko159 with real Services.prefs and the unmodified RPC direct-services path, no RPC/Services mocks. Cases cover both initial mismatch directions and stale horizontal↔multirow unrelated favicon saves. An explicit external preference writer models the other window for these API cases; **this is not a two-window UI race reproduction**. Missing-native-pref behavior was not manufactured by deleting Gecko's default preference.

The patch applies to exact2950 and main+2950, and resulting source bytes equal the API-tested transformed source. Source/bundle hashes, reproduction script, result validator and protected normal-exit evidence are under proposal-tests/. No owner worktree, source branch, product resource or original PR was changed. Before integration, update the existing deliberately inconsistent observer test fixtures and add the new cases to the repository suites; neither an integrated amended full product nor a standard CI run of this amendment is claimed. See code-review.md for precise fixture semantics and further missing tests.

## Keep CI and product origins separate

- **Existing2950 CI:**39378926878 / artifact12244273619, exact69d306b1, authenticated ZIP SHA256d542e4c668ef814f2bc0fb634d8ccc58ffaa611215ecca8f0b5ee6aa64f842fe. Actual266modules pass/0fail; helper, observer and settings integration modules executed. Runtime158.0.1 / BuildID20261014104253 / daily1155, distinct from current GUI tests. This old success does not cover the newly demonstrated gaps.
- **Current GUI baseline:** authenticated unsigned complete package42077174018 / artifact13439144608, source2deb165d, native159.0.1 / BuildID20261103153232. Its relevant design/workspace/RPC/lock Git blobs match main29a3 (scope.json); the full package's overall source is still2deb165d, not silently renamed main29a3.
- **Existing2950 GUI candidate:** standard production-generated chrome/config and complete settings-page assets from main29a3+canonical8-file patch, substituted into a separate baseline copy. Other resources/native/PWA/startup bytes remain unchanged. This is **not a full candidate product build**; source patch SHA256b4fc186910cc0d76d3d81a0e5aef8c56a09bde9607f8d2d4b2b6bd1d7b778ebb.
- **Amendment:** exact-source API bundle comparison only; not a shipping product or new full-build CI.

## Remaining work and scope

Existing2950 needs the initial reconciliation and stale nonvertical save amendment, corrected/integrated regression tests, a final amended full product/CI, original macOS Photon qualification, and optional actual pending-save race/overflowing multirow coverage. Broad field-level cross-window atomicity is not claimed by the small style proposal. The original seven Linux records and KAN-7's unmet GPU conditions remain intact. No merge, release, distribution change, Runtime-lock change or Done transition was performed.

Jira final evidence/proposal comment10399 records the measured split and retained conditions. Issue remains in progress; canonical2950 head remains69d306b1. Original7matrix/22denominator and KAN7conditions remain unchanged.
