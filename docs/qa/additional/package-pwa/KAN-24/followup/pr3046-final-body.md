Depends on Draft #2950 at fixed head `69d306b11c9e52da9e6270870740d49b1d561659`. The base branch is an unmodified mirror of that head; this is a supplement for #2950, not an alternative fix. Do not merge independently or before #2950. The author's fork is unchanged.

An existing profile with native horizontal tabs and stored vertical tabs remains inconsistent until a preference change (and the reverse initial mismatch also persists). Reconcile once when registering the observer. An unrelated save from a stale Hub form can also overwrite a newer horizontal/multirow selection; resolve from the latest persisted style unless the user explicitly edits tab style.

Adds browser regressions for both initial mismatch directions, both stale horizontal/multirow directions, and explicit vertical/multirow edits. Matching/multirow observer fixtures now start in genuinely matching native states. Only two production files and two test files change.

Validation:
- Fixed dependent source `d058d700`, Runtime158: [standard CI42093984715](https://github.com/Floorp-Projects/Floorp/actions/runs/42093984715) success,267browser modules/0fail.
- Latest main29a3 + #2950 + this supplement at `493d2974`, Runtime159: [standard CI42094068516](https://github.com/Floorp-Projects/Floorp/actions/runs/42094068516) success,284modules/0fail. The observer/settings modules (7/3 declared cases) executed successfully. An initial hosted-capacity failure acquired no macOS runner; only that job was retried successfully.
- Complete unsigned production packages: [main baseline42093687541](https://github.com/Floorp-Projects/Floorp/actions/runs/42093687541) versus [integration42094061748](https://github.com/Floorp-Projects/Floorp/actions/runs/42094061748). Six actual-product runs on Debian/X11 with real synthetic UI input, native159.0.1/BuildID20261103153232 and sandbox6. Both naturally-created mismatch profiles reconcile at cold startup with all tabs blank/no restored Hub. Live bidirectional sync, settled new windows, Workspace switches and multirow through normal restart pass in both migrated profiles.
- The exact shipped settings ESM is imported from each unmodified complete product: stale unrelated-save cases fail2/2 in baseline and pass2/2 in integration. Real getter/save/RPC/Services, no replacement source or mocks. This is an API contract comparison using a real external preference writer, not a two-visible-window autosave timing reproduction.
- Ten normal quits across six completed runs, all product hashes unchanged, no remaining owned processes. Both CI and package artifact ZIPs authenticated against GitHub API SHA256.

[Reports, machine matrices, replay scripts and screenshots (immutable QA commit a411cb5a)](https://github.com/Floorp-Projects/Floorp/tree/a411cb5af57b6581667c75f52211ba0fbc18b8ef/docs/qa/additional/package-pwa/KAN-24/followup)

The complete package uses the existing production application/package pipeline with locked prebuilt native Runtime; it is not a fresh Gecko rebuild or strict final-release/helper/XPT/signed-distribution qualification. No Runtime lock, workflow, native helper, signing or distribution changes. The unchanged workflow's existing empty auxiliary-archive relaxation is recorded rather than claimed as strict validation.

Remaining: original macOS Photon (not accessed while its owner is active), two-visible-window pending-save timing, and many-tab multirow overflow geometry. Canonical source158 CI and latest integration159 products remain distinct evidence. Tracks KAN-24 / #2949; Jira comment10403. No merge/release/Done transition.
