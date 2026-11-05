# KAN-26 non-Photon CSS contracts and scoped Lepton fix

Read-only audit source: Floorp main29a3eb23145da90706d8a94441373278521cc737 and locked Runtime8965ffc346d426ccdf919bd18585e4d4a0c21654. Both audited source worktrees were clean. source-evidence.json records11 file/blob/SHA256 subjects and exact numbered excerpts. Canonical3041 head1c37f3f2 changes only Photon-pref-gated compatibility and tests; its already qualified Photon comparisons were not repeated here.

## Existing design intent

Lepton and Firefox UI Fix/protonfix load the same vendored leptonChrome.css, with separate user.js pref sets. Both explicitly enable userChrome.tab.container and disable photon_like_contextline. The vendor deliberately hides .tab-context-line and replaces it with the bottom border of .tab-content::before. It uses opacity0.75/width25% for an inactive container and opacity1/full calculated width for a visually selected one. Therefore display:none on the native line is intended, not a defect by itself. Collapsed vertical/pinned tabs have a separate fixed16px underline; expanded vertical tabs use the same calculated selected width. Optional close-button/attention preferences can intentionally hide or change the alternate line.

The alternate selected width and left offset use old --tab-inline-padding/--tab-inner-inline-margin tokens. Neither is declared in the audited vendored sheet, existing alias shim, or audited locked native tab sources. Runtime159 provides --tab-padding-inline and --tab-margin-inline-inner. The parent independently measured selected alternate width0 with visible inactive25% in real product horizontal and expanded-vertical Lepton/protonfix tabs. That actual result establishes the defect; this audit alone did not.

Fluerial has no explicit identity/container-color override. It inherits the native .tab-context-line inside .tab-background, while its selected .tab-content gets an opaque toolbar fill. Computed positive colored line geometry can still be occluded by that later sibling. The existing Fluerial group-line compatibility code specifically addresses similar layering for .tab-group-line, not container lines. Fluerial changes remain the parent's separate actual-paint investigation; this commit does not change it.

Proton returns no theme-specific sheet/user.js and retains the native container line. All designs receive the global alias sheet before theme-specific styles. Lepton-family compatibility follows the vendor sheet; Fluerial's color/shape supplements are separate. See the css.ts/browser-design-element.tsx excerpts for exact order and content AGENT_SHEET versus chrome DOM styles.

## Delivered local source commit

53071c62422c82e0e60fd40c49de15415f1d0552 on codex/kan26-lepton-indicator, independent main29a3 base, two files only:
- lepton-compat-152.css.ts adds two native-to-legacy aliases on the actual container .tab-content consumer, gated by container=true, photon_like_contextline=false and tabbar.as_titlebar=false.
- New lepton-container-geometry.test.ts imports actual vendored CSS plus compatibility into a closed-shadow layout fixture. It declares only native padding/margin tokens on the consumer, exercises two colors, inactive/selected/multiselected/selected+multiselected, CSS hover locking, horizontal and expanded/collapsed vertical attributes, ordinary-tab control and all three preference gates.

Vendor CSS and global alias table are unchanged. The consumer-local aliases avoid inheriting an already invalid root-level variable and do not target Photon or ordinary tabs. Combining this independent fix with3041 requires retaining both compatibility constants in the combined export; there is no semantic dependency on the Photon fix.

## Verification and limits

Frozen offline Deno check, formatting and git diff--check passed. A temporary node_modules symlink was used only for type resolution and removed without modifying its shared target. The source worktree is clean after the local commit. No push/PR/Jira or source edits outside this independent worktree were performed.

Authoritative real-Gecko regression fixture evidence: test-final/result.json. Same immutable full native159.0.1/BuildID20261103153232 product, normal DBus, own Xvfb111 and profile, sandbox6, no disable environment:
- Baseline compatibility: first selected-width regression fails expected216px/actual0px.
- Candidate compatibility: both runAllTests cases return successfully (null is the Marionette serialization of resolved void), including32 state/color/orientation/hover combinations inside the geometry case plus collapsed-vertical and scope controls.
- Normal in-app quit, exit0, full product hashes unchanged and owned processes0.

This is a real browser CSS regression fixture running compiled test modules, not a candidate product build, native-window geometry qualification, physical hover input, original Windows158 reproduction or screenshot-level product acceptance. The parent handles actual product/full-package verification and Draft submission.

Earlier test-run diagnostics remain retained: immediate pseudo widths sampled during a registered200ms transition, a diagnostic use of unavailable InspectorUtils.getCSSStyleRules, missing async pref restyle barriers, and an HTMLDivElement type annotation rejected by Gecko's createElementNS typing. The final fixture waits350ms after state changes and uses HTMLElement. These are test/procedure issues, not additional product defects. Older copied runner metadata incorrectly labeled display101; actual runner commands used111, and authoritative test-final correctly records111. No protections were weakened to obtain a pass.

Publication must exclude every home-direct/ profile/runtime directory inside this tree. Only source audit, scripts, compact JSON/logs and test bundles are needed.
