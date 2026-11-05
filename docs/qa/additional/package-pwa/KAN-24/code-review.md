# KAN-24 / issue2949: canonical Draft2950 static review

Reviewed exact fork head69d306b11c9e52da9e6270870740d49b1d561659 against base7b72dbf35d9b551aebf2d8bdada943660880b910; latest main29a3eb23145da90706d8a94441373278521cc737. Read both AGENTS and all eight changed files, issue/PR descriptions, review and inline comments. No source edits, PR changes, comments, new PR or browser tests in this assignment. Actual product UI verification is separately owned. Existing2950 remains canonical.

## Architecture and retained behavior

The profile-wide sidebar.verticalTabs boolean is authoritative for vertical versus nonvertical layout. The pure helper resolves true to vertical, false demotes vertical to horizontal while retaining multirow/horizontal. A per-window chrome config signal mirrors floorp.design.configs; its Solid effect writes JSON to the profile preference, and the preference observer propagates cross-window config changes. The added JSON equality check suppresses observer/write-back loops without changing signal equality for existing callers.

The new native-pref observer bridges native toggles into this signal. Settings read the effective native style, queue their own writes, stop writing sidebar.revamp, and refresh on focus/visibility while guarding unsaved edits and overlapping reads. Firefox may itself modify sidebar.revamp; checking that it stays absent in the mocked settings fixture proves only that Floorp RPC did not write it. Do not interpret unchanged revamp as a real-Gecko invariant without observing native behavior.

## Confirmed source gaps

1. Initial native state is not reconciled. utils/vertical-tabs-observer.ts registers at line46 but never invokes onPrefChange. configs.ts lines291-313 initialize from stored JSON and375-389 only register the observer. With stored vertical/native false, config remains vertical indefinitely until a later native change. Hub getDesignSettings can display horizontal meanwhile, so a correct Hub radio alone does not prove config/CSS healing. Reverse mismatch (stored horizontal or multirow/native true) is also unreconciled. This independently validates existing inline review4346185151/review5611379353; it is not a new duplicate finding.

2. An unrelated save can overwrite a newer nonvertical style from another settings window. dataManager.ts lines144-150 reads fresh storedStyle but resolves native against requestedStyle from the stale form. Example: A still shows horizontal; B saves multirow; native remains false; A changes favicon color with hasTabStyleChanged=false. Current resolver(false,horizontal) returns horizontal and JSON overwrites fresh multirow. The reverse stale multirow versus newest horizontal also regresses. When the native pref is unavailable, requestedStyle similarly replaces the current stored style. Existing tests heal vertical/native-false and protect native true; they cannot catch horizontal-versus-multirow ambiguity because both share native false. The local write queue serializes one page instance, not other windows or native changes.

## Smallest proposed amendment (artifact only)

additional/KAN24/minimal-fix-proposal.patch adds synchronous initial onPrefChange after registration and uses fresh storedStyle whenever hasTabStyleChanged is false, resolving that against native if available. Explicit style edits still use requestedStyle. The patch passes git apply --check against exact2950 but is NOT applied or runtime-tested. It is a proposal for the existing PR owner, not a replacement PR.

Adding initial reconcile requires updating existing observer test fixtures: testMatchingStyleIsNoOp currently starts stored vertical/native false; that must first reconcile under the new contract, so initialize matching true/vertical instead. testTurningVerticalOffKeepsMultirow currently starts native true/multirow; initial reconcile would correctly become vertical, then off becomes horizontal. To test preserving multirow, begin native false/multirow and verify initial/no-op state. Do not adjust expected behavior merely to keep these inconsistent fixtures passing.

## Missing meaningful tests for canonical2950

- Real-pref observer setup with stored vertical/native false: immediately horizontal, one apply, no pref toggle; reverse horizontal/native true; matching state no apply; native false/multirow preserved; unavailable/wrong-type pref preserves stored value; cleanup stops updates.
- Actual config startup from preexisting mismatched profile, before importing configs: assert config, persisted JSON and displayed tabbar agree without Hub opening. Helper tests alone do not cover initialization ordering or per-window Solid effects.
- Two settings windows, A stale horizontal, B changes to multirow while native false, then A changes only favicon: preserve latest multirow, do not write native or revamp. Reverse direction and missing-native-pref case. Add analogous mock-RPC integration assertions but retain real two-window UI test.
- native toggle while a Hub tab remains visible: current page refresh is focus/visibility-driven, not subscribed live; verify expected UX and that an unrelated save cannot silently restore stale state. Explicit later user style selection should intentionally win.
- Native change during queued/read-write RPC interval is still not atomic; pause RPC in a controlled test and check latest native state before claiming race safety. The proposal addresses confirmed stale-snapshot selection, not all cross-process races.

## Latest main and workspace implications

Direct git diff from2950 base to latest main is empty for the designs directory, design settings source, and settings integration test: neither confirmed gap is fixed there. Main has later workspace/container and sync changes (b226a52a,50111e54,aac9cf5e and related commits). Scoped searches in current workspace chrome/modules find no tabbarStyle, floorp.design.configs or sidebar.verticalTabs reference, so no direct workspace preference ownership collision is demonstrated. Workspace UI depends on actual native layout; test switching workspaces with vertical, horizontal and multirow plus tab/container assignment retention, but do not claim a workspace defect from this static change alone. Profile-wide layout across windows is expected, not per-workspace design state.

Source hashes for eight reviewed files are in additional/KAN24/reviewed-files.json. Classification: two confirmed code gaps; runtime impact remains for actual-product verification. No new product reproduction or pass claim is made by this report.

## Follow-up actual-source API validation

See proposal-tests/report.md, matrix.json and result.json: canonical fails four tested API conditions; the reviewed proposal passes all four using actual Gecko Services prefs and unmodified direct RPC. This upgrades those code paths to API-reproduced evidence, not Hub UI reproduction or an integrated full-build pass. proposal-tests/patch-apply-checks.json also verifies exact2950 and latest-main-plus-canonical-stack applicability and byte equality with API-tested proposed source.
