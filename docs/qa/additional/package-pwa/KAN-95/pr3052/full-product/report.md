# KAN-95 complete Linux candidate comparison

Exact PR3052 head `214ba6657a63314594de3867785441558b7a63b5`, base
`29a3eb23145da90706d8a94441373278521cc737`: natural-style complete-product
comparison succeeds **919 checks /0 failures**. Both complete products were
actually launched; no CSS was injected or product resources replaced.

The new matrix has24 configurations per product (48 total),72 tab backgrounds
per product: Proton/Lepton/Photon/Fluerial × horizontal/actual expanded native
verticalMode × normal/compact/touch. Genuine blue container selected, orange
container and normal tab inactive. Actual Hub synthetic WebDriver clicks,
tabbrowser selection and clear-multiselect, pointer moved off tabs. No hover,
multiselection or physical-input coverage is claimed. Original39 Linux states
were not rerun; these new density/native-orientation/Photon conditions supplement
that evidence. Raw baseline input inherited a coarse earlier description;
baseline-report.md clarifies it without rewriting the raw result. Candidate
input describes this actual narrower coverage.

## Measured result

| Design/orientation | Density | CSS top margin, base→candidate | CSS bottom margin | Actual paint geometry |
| --- | --- | --- | --- | --- |
| Lepton horizontal | normal/compact | **4→2px**, allthree roles | Existing0px retained | Topgap4→2; background height36/32 unchanged; geometric endgap−4→−2 |
| Lepton horizontal | touch | 4→4px |0px retained | Both gaps/height unchanged |
| Lepton vertical | allthree |2→2px |2px retained | Both gaps/height unchanged |
| Proton horizontal/vertical | allthree |4/2px unchanged |4/2px unchanged | Both gaps/height unchanged |
| Photon horizontal/vertical | allthree |0px unchanged |0/2px unchanged | Both gaps/height unchanged |
| Fluerial horizontal/vertical | allthree |4/0px unchanged |0px unchanged | Both gaps/height unchanged |

Lepton legacy root token remains2/2/4px; the candidate native root token follows
it only horizontally. Native vertical token2px and all other designs stay at
their baseline values. Alltab backgrounds retain positive area. Density is
verified both as effective browser.uidensity and actual root uidensity; design
saved state/native orientation/effective gates/default theme/fonts ready and
genuine container identities/actual selected flags are checked. Candidate also
records Hub checked radios explicitly; baseline captured the actual clicks,
saved state and native rendering, not that additional checkbox snapshot.

Two mistaken preliminary expectations were corrected **before any candidate UI
observation**, with initial criteria retained. Existing vendor rule
leptonChrome.css7063–7066 forces horizontal margin-bottom0!important, so the
alias must not change that value to2. Separately, CSS bottom margin0 is not
geometric bottomgap0: baseline background y4/height36 and stack y0/height36 give
endgap−4. The final criteria require topgap4→2 and preserve the CSS bottom
override, while recording changed-case end geometry rather than forcing a false
zero/invariance assumption. Other conditions require both real gaps unchanged.
See criteria-initial.json, criteria-adjustment.json and pre-candidate-checker.json.
All five frozen probe/comparison/criteria/line-analysis files match the recorded
pre-candidate hashes after execution; no candidate-driven assertion weakening.

## Container lines are a separate observation

The same screenshots/actual style rectangles provide48 indicator observations
per product using pixel centers inside the actual fractional indicator.
Correct color/opposite color and genuine blue/orange identity are measured.
Lepton uses its intended alternative pseudo bottom border; others use native
contextline. This does not interpret display:none alone as failure.

| Design | Visible base→candidate /12 |
| --- | --- |
| Proton |12→12 |
| Lepton |6→6 |
| Photon |0→0 |
| Fluerial |9→9 |

New visible-indicator losses **0**, new wrong-hue observations **0**. The21
preexisting missing indicators remain. Both subjects exclude PR3041/3048; their
known selected-line defects are not3052 regressions or claimed repaired by this
spacing change. line-comparison.json is independent from margin qualification.

## Product identity and conservation

- Baseline complete source29a3 package artifact13446089131; ZIP SHA256
  `05164de082b5b6c1f18ec70155e6ae4eebec14486cb983b1a013660fbf00239d`.
- Candidate complete package run42123754997, attempt1 success,
  artifact13459714736; ZIP SHA256
  `d4e89cc7ba88751df704771c89c93017a03a01aaa8877db0f538a62c8aa81502`,
  inner TAR `f1404a3d8d23058d6eba83b0875cbc4154bf756e9244c8a32856b9409d961ee4`.
- Root authenticated API/source/run/digests, fresh safe extraction and inventory;
  comparator independently matches both recorded pre-run product file maps to
  authenticated inventories. This is the existing complete application/package
  pipeline using locked prebuilt native Runtime8965, not fresh Gecko compilation
  or signed-release qualification.
- Both live products: native159.0.1/BuildID20261103153232, actual executable and
  dedicated profile paths, sandbox6/no disable flag, default theme/DPR1.
  Debian13/Xvfb117/X11/private normal DBus, no window-manager/OS-edge claim.
- Both normal unforced in-app Quit, browser exit0, zero owned processes, display
  cleaned. Every product regular-file hash and complete directory/symlink tree
  is unchanged before/after. Dedicated completed profile/HOME/runtime caches may
  be reclaimed after guards; raw measurements and products are preserved.

Read-only regeneration commands are in README.md. StandardCI42123057521 and
Windows42123152394 are root-owned and were not touched or inferred from this
Linux result. Colocated source tests/diagnostic DOM sheets belong to separate
evidence and are not counted here as complete-product execution.

Remaining: original Windows10/12.17.0/native OS titlebar, physical input/GPU,
highDPI, other custom themes/explicit theme overrides beyond source tests, and
hover/multiselection/collapsed-vertical coverage beyond these24 states. The
specific horizontal Lepton regression is verified in this limited complete
Linux product; the whole original report is not declared fixed/Done. No source
change, merge, release or Jira/PR edit was made by this probe agent.
