# KAN-26 non-Photon actual-product measurements

Initially, two unmodified complete products were exercised through **40 non-Photon states each**: main29a3/native159.0.1 and original-version Floorp12.20.2/native158.0.1. A later current159-only48-state interaction supplement brings the current baseline to88 distinct states; the old158 comparison remains40 (details below). No Photon case from the existing36-case acceptance was repeated. Each product remained byte-identical, quit normally/exit0/forcedfalse, and left no owned processes or X107 socket.

These are Debian/Xvfb107/X11 measurements, not original Windows qualification or physical-GPU/high-DPI acceptance. Current full-package source29a3eb23145da90706d8a94441373278521cc737 originates from authenticated run42093687541/artifact13446089131. Old product tag576bc4bbca058e4c24dc5bd2c7526d64cedc94cc and archive were authenticated by parent; live identity is158.0.1/BuildID20261014104253. Current live identity159.0.1/BuildID20261103153232. All actual runs retain content sandbox6 and absent disable environment.

## Genuine actions and captured state

The installed Hub's real design/style radio controls are clicked using WebDriver to choose Proton control, Lepton, Firefox UI Fix/protonfix and Fluerial, then horizontal/actual native verticalMode. Genuine ContextualIdentityService creates blue/orange containers and real userContextId tabs; a non-container tab is the control. No SSB/identity mocks, generated styles or product overlays are used. Tab selection uses native tabbrowser selection APIs; multiselection uses native addToMultiSelectedTabs; hover uses real WebDriver pointer input. All eight design/orientation hover snapshots independently record actual `:hover=true` on blue.

Each orientation captures normal selected, blue selected, orange selected, inactive-blue hover with orange selected, and genuine blue+orange multiselection with blue hover. Snapshot attributes prove selected/multiselected, actual verticalMode, default effective theme and identity classes. Native context-line, tab background/content/stack, their pseudo-elements, borders/shadows, inherited identity colors and both old/new padding custom properties are captured.

`PIL.ImageGrab` captures the **actual X11 root including browser chrome**. This avoids assuming a WebDriver content screenshot contains the tabstrip. `analyze.py` records whole-tab hue counts and separately isolates the actual native indicator or Lepton alternative border ROI. Generic colored multiselection outlines are not counted as proof of an indicator. The image coordinates add each snapshot's real `mozInnerScreenX/Y` origin (current159=0,0; old158=4,4); this instrumentation correction was made before the version conclusion. Raw JSON/screens are unchanged. Counts use documented blue/orange hue thresholds, not exact arbitrary-theme screenshot reference matching.

## Reproduced defects and explicit non-reproductions

Both releases produce the following same bounded results for blue; orange has the same selection-dependent behavior:

| Design/state | Horizontal actual indicator pixels | Expanded vertical actual indicator pixels |
|---|---:|---:|
| Proton blue selected |426 |48 |
| Proton blue inactive |426 |48 |
| Lepton blue selected |0 |0 |
| Lepton blue inactive |110 |124 |
| protonfix blue selected |0 |0 |
| protonfix blue inactive |110 |124 |
| Fluerial blue selected |0 |20 |
| Fluerial blue inactive |402 |20 |

Lepton/protonfix deliberately hide native `.tab-context-line`, so that fact alone was **not** graded as a bug. The intended `.tab-content::before` alternative has identity-colored2px bottom border and opacity1 selected, but its computed width is0. The inactive width is55.25px horizontally or61.9167px vertically and actually renders110/124 corresponding pixels. The two referenced legacy custom properties are empty; native replacement tokens are present. Actual captures show the selected indicator absent while inactive indicators remain. This is the specific supported defect, rather than a demand to restore the intentionally hidden native element.

Fluerial horizontal selected/multiselected native line remains201x2, but its actual ROI contains **zero** identity-colored pixels. Its selected tab-content is opaque and later at z-index0; inactive transparent content reveals402 pixels. Thus computed visibility/geometry alone would have missed a real occlusion. Normal expanded-vertical selected/inactive Fluerial lines are visible2x10/20pixels, so this Linux observation does **not** reproduce the original report's selected vertical absence.

A separate Fluerial **inactive expanded-vertical hover** issue was measured: background shrinks to241.467x18.75, context-line becomes2x0 and moves outside the247.667px tab box; colored pixels disappear. A new limited supplement measured that same state at0/0.5/2seconds with actual hovertrue, excluding a transient first-frame explanation. Multiselection restores a2x10 native line; its actual ROI has about20pixels, while the larger whole-tab hue count includes unrelated selection outlines. Parent is investigating each causal scope before deciding any fix.

All40 non-container crops in each version contain zero blue/orange identity-hue pixels. Their computed styles and images remain available for a future candidate regression comparison.

## Supplement and qualification limits

`current-product/supplement-run` is a second fresh-profile, limited measurement, not a repeat of all40 cases. It records actual tabContainer `expanded=""`, default density and absent closebuttons attribute for the vertical states. The effective userChrome preference sample records photon-like-contextline=false, close_button_at_hover=true and Lepton padding=true. Container enablement also follows from the actual gated alternative rendering, but the initial prefix-filtered preference sample omitted the direct container key; do not claim that omission as a negative preference result. Future probe versions now record the effective container/contextline/close preferences and tabContainer attributes in every snapshot.

Collapsed-vertical/sidebar geometry, other closebutton/density preferences, original Windows, highDPI and physicalGPU are unqualified. No product/source/PR/Jira change was made by this measurement. Parent owns source fixes, test integration and full fixed-package qualification.

## Retained harness failure and replay

The first attempt called obsolete `gBrowser.setTabLabel` and failed before any design matrix state. It quit normally with product unchanged/owned0 and is retained under `attempt1-obsolete-label-api/`. The corrected probe merely sets a temporary test label attribute (native updates may replace it with New Tab); roles are tracked by a harmless custom data attribute. No protection or product behavior was changed to repair the harness.

Replay from a new dedicated output directory:

```
dbus-run-session -- python3 probe.py --product <authenticated-product>/floorp --source-sha <actual-source> --out <fresh-output>
python3 analyze.py <fresh-output>
```

`probe.py` creates fresh HOME/profile/runtime and owns display107/port2930; never publish those directories. Publish scripts, result/identity/product inventories, raw style snapshots, reviewed root screenshots/crops, pixel-matrix and version-comparison only. `summarize.py` validates both completed initial runs and regenerates the descriptive version comparison.

## Historical input metadata clarification

Initial result JSON uses the coarse input phrase “WebDriver pointer hover/multiselect”. The actual source and recorded state show **native tabbrowser API selection/multiselection**, while Hub radio input and pointer hover are WebDriver operations; all are synthetic. The shorthand is not a claim that Ctrl-click generated multiselection or that physical input was used. Historical raw JSON is retained unchanged. Future probe executions use the explicit input description above.

## New active-hover and selected-multiselection coverage

A separate fresh-profile **48-state** supplement fills the original40-state matrix's specific interaction gaps, without replaying any original state or Photon condition. For every four-design × horizontal/actual-expanded-vertical combination it measures: active blue hovered, active orange hovered, blue selected with inactive orange hovered, both multiselected with blue primary/hovered, both multiselected with orange primary/hovered, and hovered selected normal control. Each snapshot records requested interaction and actual selected/multiselected/hover flags; all48 independent interaction checks agree. Effective container/contextline/closebutton preferences and actual tabContainer attributes are now recorded in each new state.

The current159 baseline is therefore **88 distinct states across two fresh-profile runs**, merged by unique case ID in baseline-88-pixel-matrix.json. The previous old158 comparison remains40 states; no old158 active-hover supplement is claimed. The baseline remains a descriptive known-defect measurement, not88 product-pass assertions. Active hovered selected blue/orange and selected-primary multiselection still expose the Lepton/protonfix selected alternative-width defect in both orientations and Fluerial selected horizontal occlusion. The new inactive-orange expanded-vertical hover also collapses Fluerial's native line, complementing the earlier blue case. Proton indicators remain visible. All88 normal-control crops contain no blue/orange identity-hue pixels.

The new run quit normally with product hashes unchanged, sandbox6/disable-env absent, owned processes0 and X107 cleaned. No old binary or completed40-state case was rerun. CLI `--extra-hover-only` selects exactly the new48; `--all-states` selects original40 plus extra48 for future complete fixed-product acceptance. The default remains original40 for compatibility. Analyze each output with analyze.py; it selects indicator versus alternative by the actual design and tab role, not case-name assumptions. The new supplemental result still contains the prior coarse input wording because its process loaded the script before that text correction; native API selection/multiselection is explicitly recorded here and in baseline-88-summary.json without rewriting historical raw evidence.
