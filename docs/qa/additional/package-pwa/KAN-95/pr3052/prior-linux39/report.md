# KAN-95 / GitHub2670 — current Linux tab spacing

**Classification: visible background inset observed; no new unintended Floorp defect isolated. Original Windows condition remains unverified.** No product patch or new PR was made.

## Context and versions

Latest Jira issue and comment10145, original GitHub2670 body/comments (none), and matching existing PRs were read first. Original report: Floorp12.17.0, Windows10, Lepton/Fluerial top space; window/titlebar configuration unspecified. PR2854 fixes a different Boolean-titlebar visibility issue2822, not established as2670's fix. No directly verified canonical fix PR for2670 was found.

Source29a3eb23145da90706d8a94441373278521cc737 (latest main confirmed). Clean isolated branch qa/kan95-theme-spacing at that SHA in /workspace/kan30-regression-source. Existing test/PR3043-related commit bfb8e5b5 preserved on test/sidebar-hover-regression-pr2934; no shared/canonical source change. Official Floorp12.21.1 / Gecko159.0.1 build20261103153232, Firefox159.0.1 build20261102144711. Product binary hashes recorded.

Debian13.6/X11, private Xvfb101, actual Openbox WM, software WebRender, isolated profiles/DBus. No sandbox/signature/TLS verification disabled. Native focus behavior restored with focusmanager.testmode=false because Marionette's recommended test default otherwise changes deactivation behavior. This is not Windows, KDE or Wayland coverage.

## Actual UI and geometry/paint tests

39 layout observations: Floorp24 (fresh default Lepton plus visible UI-selected Proton/Lepton/Fluerial, normal/maximized, menubar hidden/visible and titlebar configuration fixtures), Firefox6, native-CSD Floorp6/Firefox2, and one full-production rebuild control. Theme selection used W3C pointer clicks at the visible184px-high label cards, never the hidden1px radios; checked radio and stored design were asserted. Applied design is independently visible in cropped screenshots/navbar icon shapes, tab heights (Lepton36px/Proton44px/Fluerial40px), and Lepton's legacy tab-margin variable. Cascade inspection additionally proves the Fluerial supplemental sheet is loaded and matched.

Titlebar/menubar variants use existing native configuration command/pref fixtures, not CSS injection and not a claim of clicking every corresponding UI checkbox. The first minimal Openbox session lacked XDG_CURRENT_DESKTOP and used a real server titlebar (20px normal/19px maximized); inTitlebar0/2 did not enable CSD there. This is environmental capability evidence, not a product failure. Setting XDG_CURRENT_DESKTOP=openbox truthfully identifies the actual WM and makes Gecko enable real GTK CSD. Native gtkCsdAvailable/customtitlebar and zero frame extents were observed; no Windows media forcing.

Results, relative to tab toolbar:

| Product/design | Tab outer hitbox top | Selected background top |
|---|---:|---:|
| Floorp default Lepton / Proton / Lepton / Fluerial |0px|4px|
| Firefox control |0px|6px|
| Current-main full-production rebuild Lepton |0px|4px|

Visible menubar moves the entire tab row down25px; it does not add a gap within that row. Normal/maximized results agree. All20 maximized observations independently verify both EWMH MAXIMIZED atoms and Gecko sizemode. OS frame extents, client screen coordinates, window geometry, computed margins, rects and top40-row paint columns are saved. Screenshots were visually inspected.

Five OS XTest control fixtures are distinct from W3C theme selection and from physical input. Without CSD, clicking client tab-row top selects another QA tab, but absolute screenY0 hits the real19px OS titlebar; Firefox behaves the same. With native CSD/maximization, absolute screenY0 selects another QA tab in both Floorp Lepton and Firefox, with center-click positive controls. The unrelated rebuild control also passes. Thus observed4px/6px background paint inset is not an unclickable top-edge gap in these current Linux cases.

## Fluerial cascade contradiction resolved

Base fluerial.css has nested `.tab-background { margin-block:0 !important }`. Current getCSSFromConfig loads a subsequent `FLUERIAL_TAB_CORNER_CSS` supplement. That source explicitly says **Restores --tab-block-margin top gap**, and its stronger `#TabsToolbar #tabbrowser-tabs .tabbrowser-tab:is(...) > .tab-stack > .tab-background:is(...)` rule sets `margin-block-start:var(--tab-block-margin,4px) !important`.

Actual UI-selected Fluerial29963 records loaded supplemental inline sheet16, the root selector matching the real selected background, important declaration, empty legacy --tab-block-margin (therefore fallback4px), new --tab-margin-block4px, and computed margin-top/start4px. The selected-tab override explains why the base0 rule does not yield0. git blame traces these top-gap declarations to c4cb5ee0b, June17; they are not newly introduced by this task. `cascade-29963.json` is read-only inspection without stylesheet modification. Its leaf-selector diagnostic also lists nested rules whose parents are not evaluated; only the relevant parentless root selector is used as cascade proof.

Lepton vendor's separate optional8px drag-space path requires userChrome.padding.drag_space (and a separate maximized option); those prefs are unset in tested defaults. No8px-path reproduction or all-configurations intent claim is made. Firefox's6px decorative inset provides a comparison, not proof that every Floorp margin choice is correct.

## Rebuild control, limitations and outcome

Existing full-production package source1b8f3503df9e8cae6b08118ecbe2d85e2d5158e7 includes current main plus unrelated existing PR2934 sidebar fix only. One actual UI-selected Lepton maximized-CSD case29961 verifies its own GreD, reproduces4px, and passes screen-top/center clicks. It is a **clean rebuild control, not a KAN95 fix candidate**.

Original Windows10/12.17.0→current behavior, native Windows titlebar settings and physical input remain unmet. Current Linux visual inset alone is insufficient to label original issue fixed, Done, or a new code defect. No speculative CSS removal and no duplicate PR. Proposed follow-up narrows to original Windows condition; keep needs-repro until that condition is assessed.

Fixture failures are explicitly excluded:29951 Openbox data-directory setup,29954 inverted pref-type reader,29962 read-only inspection syntax. Legacy InspectorUtils method unavailable in edge fixtures; final inspection uses normal read-only CSSOM instead. These are harness/setup failures, not product bugs or passing cases.

## Evidence and reproduction

measurement-summary.json (39 rows/5 pointer controls), matrix.json, primary-source-summary.json, product-hashes.json, floorp-29955.json/firefox-29956.json (server titlebar), floorp-29959.json/firefox-29960.json (native CSD), floorp-29961.json (rebuild), cascade-29963.json and screenshots/. Scripts run.py/probe.py, run-edge.py/edge-probe.py, run-csd.py/probe-csd.py, run-rebuild.py/probe-rebuild.py, run-cascade.py/cascade-probe.py preserve exact commands. Private display101 is checked absent before launch and all owned groups cleaned.

Run fresh isolated profiles with PYTHONPATH pointing to existing marionette library. For example `python3 run-csd.py <unused-port> floorp full`, repeat Firefox sequentially; `python3 run-cascade.py <unused-port> floorp cascade`. Existing official products and Openbox dependency sysroot are referenced by explicit paths; adapt those for another machine. No native build/dependency installation is needed. Publish only the include list in publish-list.json. Use primary-source-summary.json instead of raw GitHub/Jira metadata; omit author/account/avatar/profile fields, runtime profiles/cache and large browser logs. Ended owned profiles/cache were removed after evidence retention. Parent owns Jira/matrix publication; no Jira comment was posted here.
