# Complete candidate88-state comparison

Do not start the UI probe until the parent has authenticated the final package.
The final expected source is0e80f14dc439dde7817c0620f0fc112d54dfba80, package
run42113006050. Source3a9 has production CSS equality but is not the default
accepted final-head identity.

```
dbus-run-session -- python3 probe.py --all-states --product <authenticated-product>/floorp --source-sha 0e80f14dc439dde7817c0620f0fc112d54dfba80 --out <fresh-candidate-output>
python3 analyze.py <fresh-candidate-output>
python3 compare_candidate.py --candidate <fresh-candidate-output> --product <authenticated-product>/floorp --intake <authenticated-intake>/product-intake.json --expected-artifact <actual-artifact-id> --out <fresh-candidate-output>/comparison.json
```

The comparator requires all88 unique IDs and real roles to match the recorded
baseline. It checks actual selection/multiselection/hover/native verticalMode,
container identity classes and IDs, effective default gates, expanded vertical
state, native version159.0.1/BuildID20261103153232 and sandbox6. It uses native
indicator ROIs for Proton/Fluerial and actual alternative-border ROIs for
Lepton/protonfix, requiring positive correct-color pixels and no opposite-color
pixels for both containers in every state. Whole-tab colored outlines are not
accepted as indicator evidence.

Proton ROI geometry/hue counts and painted/layout styles are compared to the
same baseline case. Every normal-control crop must remain without blue/orange
identity hue, and its actual styles/layout/background position/z-index must be
stable. Repaired custom-property alias values themselves are intentionally not
visual-style invariants. All differences are reported; failures are not waived.

The future probe now records full product-tree metadata (directory entries and
symlink targets as well as file hashes) before and after. This supplements the
historical file-only hash maps without rewriting them. The comparator rejects
any tree mutation, including newly created dangling links, and independently
checks the current tree and regular files/symlinks against authenticated intake.
Normal in-app unforced quit, zero owned processes and display cleanup are gates.

This is Linux virtual-display/nonPhoton/default-gate acceptance, not original
Windows/highDPI/physicalGPU or signed/final-release qualification. The checker
does not execute a browser, inject CSS or alter recorded results.
