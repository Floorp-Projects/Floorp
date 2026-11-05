# Fluerial causal AGENT-sheet diagnostic (not delivered-product acceptance)

The exact CSS in fluerial-diagnostic.css was temporarily registered through the actual nsIStyleSheetService AGENT_SHEET route in the unmodified159 complete main product. CSS SHA256:e22c5eedf3296047b73f2f546c0067047a2ef0a4e363cdda95c314d4e55b73de. Registration was measured, then the sheet was unregistered and its absence measured before normal quit. Sandbox6 and product files were unchanged. No security preference was changed.

This is an isolated causal styling diagnostic; it is not a source build, full fixed package or signed-distribution qualification. The parent must integrate source and regression tests and validate the resulting complete package.

| Observed condition | Before | Temporary proposed CSS | After unregister |
|---|---|---|---|
| Selected blue horizontal native line |201x2 geometry,0bluepixels |same201x2 geometry,402bluepixels |0bluepixels |
| Selected orange horizontal native line |201x2 geometry,0orangepixels |same201x2 geometry,402orangepixels |Not separately repeated |
| Blue inactive horizontal |201x2/402pixels |unchanged |unchanged |
| Expanded vertical blue idle |2x10/20pixels |same2x10/20pixels and exact same x/y |Not separately repeated |
| Expanded vertical inactive blue hover |2x0/outside tab/0pixels |2x5/10pixels inside tab |Not separately repeated |

The background becomes position:static/z-index:auto only on actual container tabs. The native line is absolute/z-index1 and anchored to the stable tab-stack, not the shrinking hover background. Horizontal rounded insets and selected block-start gap preserve the old correct native rectangle. Vertical insets preserve the old idle rectangle while allowing positive height in the shorter hover stack. This matches the established group-line anchoring principle without raising the entire background over labels/buttons.

Before/after horizontal blue, orange and normal-control label and close-button centers all pass actual document.elementFromPoint target-or-descendant checks. The non-container background remains position:relative; its captured styles and colored-pixel absence are unchanged. This is a hit-test check, not a close-button click operation. Actual root screenshots and tab crops were visually inspected, showing a restored selected horizontal blue line and a small visible right-edge vertical hover line.

A total11 bounded snapshots were captured, not a repeat of the full40-state matrix. The before states and selected-blue after-unregister return establish reversibility. The agent sheet is not left registered. Normal in-app quit forcedfalse/exit0, complete product hashes unchanged, owned process count0 and X107 socket absent.

Evidence: fluerial-diagnostic-run/{result.json,agent-register.json,agent-unregister.json,pixel-matrix.json,*hit-test.json,fluerial-diagnostic-*.json/png}. Replay uses fluerial_diagnostic.py with a fresh --out directory. Exclude HOME/profile/runtime from publication.
