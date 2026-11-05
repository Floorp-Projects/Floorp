# Natural complete-product KAN-95 probe

The probe launches the actual immutable product directly, not a development
bootstrap. It creates only ordinary Hub settings, genuine containers and tabs
inside a fresh dedicated profile. It never registers or appends stylesheets,
replaces resources, forces CSS media, mocks identifiers or disables protections.
Marionette is used for synthetic input/observation. Profile caches are disposable;
raw JSON/screenshots and product inventories are evidence.

```text
dbus-run-session -- python3 probe.py --product <complete-product>/floorp --source-sha <authenticated-source> --out <fresh-directory>
python3 line_analyze.py <fresh-directory>
python3 compare.py --baseline baseline --candidate candidate --baseline-intake <baseline-intake>/product-intake.json --candidate-intake <candidate-intake>/product-intake.json --out comparison.json
python3 line_compare.py --baseline baseline --candidate candidate --out line-comparison.json
```

Private Xvfb117 and Marionette port2955 must be unused. Existing sysroot and
Marionette helper paths at the top of probe.py reflect this Cloud workspace;
they are dependencies, not product modifications. Source references are fixed in
criteria.json. Baseline intake is the authenticated source29a3 complete package,
not an inferred Git SHA for a release binary. Candidate runs only after the root
has authenticated exact head214ba665/package42123754997 and installer extraction.

Read criteria-initial.json and criteria-adjustment.json for two pre-candidate
expectation corrections based on baseline measurements and static existing
vendor rules. Neither was a candidate failure waiver. The final comparator's
SHA256 was recorded before candidate access/measurement in pre-candidate-checker.json.
CSS marginBlockEnd0 and actual rectangle bottomgap−4 are different quantities.

Margin acceptance covers24 configurations and72 tab backgrounds. Selected blue,
inactive orange and inactive normal are genuine tabs. Clear-multiselect and
off-tab pointer movement are not multiselect or hover coverage. The old coarse
raw baseline input description is clarified in baseline-report.md; it is not
rewritten. Candidate input wording states the actual coverage. New candidate Hub
checked-radio observations supplement the original baseline click/pref/style
evidence without fabricating historical data.

Container-line analysis is independent. It uses the same pixel-center-inside-
element method as KAN26 and reports correct/opposite identity pixels. Missing
indicators existing before3052 are retained as known defects, not new spacing
regressions. No broad KAN26 repair is claimed from these24 configurations.

No original Windows10, physical input/GPU, highDPI or native OS-edge qualification
is implied. Existing Linux39 prior states and Windows work belong to separately
recorded evidence and are not repeated by this script.

The committed Marionette transport class is also available at
`docs/qa/linux-22/package-pwa/pwa_ui_probe.py` in the same QA branch. Map the
probe's original absolute helper/sysroot paths to the local checkout and actual
installed Xvfb/libraries when reproducing elsewhere; use Python/Pillow and
normal DBus. Such launcher-path adaptation must be recorded and does not imply
a Windows or physical-input qualification.
