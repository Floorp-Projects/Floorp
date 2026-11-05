# Fluerial container regression: minimal fix

Source commit `b1ba3ef955a8b96ae2bd9420889685c22c02e86d` changes only the Fluerial compatibility CSS and new colocated browser regression module. Vendored Fluerial and Photon source are untouched. The production block is the exact selector/property body of the independently validated diagnostic CSS, with its comment changed to describe production intent.

The container line stays inside its original DOM parent but its containing block becomes the stable tab stack. It paints above the selected content fill without raising the whole background over labels/buttons. Horizontal selected/multiselected top gap and native inline radius insets remain; vertical native margins are preserved against hover background shrinking. Identity colors are not overridden.

## Actual browser regression evidence

`run3/result.json` executes exact baseline and candidate source variants on authenticated immutable full product source29a3/native159.0.1/BuildID20261103153232, sandbox6, normal DBus/Xvfb113. The new module imports actual vendored Fluerial CSS and creates genuine blue/orange contextual identities and native tabs. It registers the fixture stylesheet at the same AGENT origin as production. An author sheet cannot override vendored AGENT-important positioning and is not a valid fixture.

Both baseline cases fail: horizontal native indicator paint order and vertical hover line area. The final candidate module completes both cases, including distinct actual identity-color assertions, selected/multiselected paint order, vertical hover area and in-tab bounds, and a non-container color control. Multi-selection CSS is exercised by its real native attribute; product-level native multi-selection and pointer inputs are separately measured by the parent. Fixture hover uses InspectorUtils pseudo locks and is not a physical-input claim.

Normal in-app quit exit0/forcedfalse, complete product hashes unchanged and owned processes0. Cached Deno check, lint, format and diff-check pass. This is actual-browser source regression evidence, not a delivered candidate package or original Windows qualification. Parent performs full package validation.

An earlier final-color-assertion run2 was interrupted by environment/tool-session loss before a result. It is unqualified and retained; it did not modify the product. Its owned stale X112 display was removed only after the recorded owner was defunct and the socket could not connect. `run2/interruption.json` records the evidence. The first source module attempt already showed baseline2fail/candidategreen; final run3 additionally confirms exact identity colors.

Replay: node run3/build_test.cjs; dbus-run-session -- python3 run3/run_test.py --product <authenticated product path>, with fresh output/profile and unused owned display. Exclude home-direct/ and runtime-direct/ in every attempt from shared QA; retain scripts, result/validation JSON and logs only.
