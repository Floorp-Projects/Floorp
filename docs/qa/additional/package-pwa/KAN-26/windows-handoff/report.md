# KAN-26 Windows complete-product handoff

Windows execution belongs to the local coordinator. This Cloud host is Debian,
and does not execute or qualify a Windows product. Existing Linux216 main states
and18 supplemental states were not repeated for this handoff. No code changed.

## Fixed subjects and one package request

- PR3048 source: `0e80f14dc439dde7817c0620f0fc112d54dfba80`.
- PR3048 base and canonical Photon PR3041 head both:
  `1c37f3f2c53c5b213e679321e0116d4c7474661f`; both remain Draft/unmerged.
- Immediately before dispatch, all package-workflow runs at this exact source
  contained only successful Linux run42113006050. No Windows run existed.
- Exactly one Windows unsigned package-only request was accepted:
  [run42120232936](https://github.com/Floorp-Projects/Floorp/actions/runs/42120232936).
  `dispatch.json` fixes source, platform and all Runtime identities;
  signing=false, signed_validation=false, stable-v2-validation, beta=false.
- Runtime: `Floorp-Projects/Floorp-Runtime`, run41664957250, source
  `8965ffc346d426ccdf919bd18585e4d4a0c21654`, BuildID20261103153232.
- Manifest artifact13274392012 ZIP SHA256
  `2b6a1b20190dc7016957ca25f72f8e83f267de51458b515153fd209bb90d9138`.
  Downloaded manifest and actual API metadata agree on Windows target
  artifact13270584849, `floorp-windows-x86_64-moz-artifact`, digest
  `9eb4b08f2da5cdd535232edeb37ddd3866d47492e17f5921e115eb4a4f7180c6`.
  This is the existing complete application/package pipeline with locked
  prebuilt native Runtime, not a fresh Gecko compile or signed distribution.

The single run completed successfully on its first attempt. Final artifact
verification is recorded below.
The job-generated Runtime source-provenance artifact13458585692 was also
downloaded and authenticated (ZIP SHA256
22ec0f1dae3a17622ebf082609f742a3034c4b47891c7f36a6c18aa5e9fd24cb).
It describes prebuilt native Runtime8965, independently of the Floorp
frontend/application source0e80. No native recompilation is inferred.

## Reproduction material

Prior Linux evidence fullSHA:
`3235cf0c0ebe31ea79763dd3f2705410949fbaeb`.
Repository: `Floorp-Projects/Floorp`, branch `codex/qa-linux-package-pwa`.
All paths below are relative to `docs/qa/additional/package-pwa/KAN-26/`:

| Material | Path |
| --- | --- |
| Scope, findings, CI/full-package distinctions | `report.md`, `matrix.json` |
| Main Linux probe | `current-product/probe.py` |
| Pixel ROI analysis | `current-product/analyze.py` |
| Strict88 comparison, preserved false grade | `current-product/compare_candidate.py`, `full-product/comparison-center-roi.json` |
| Separate9-state normal-control comparison | `current-product/noncontainer_diagnostic.py`, `current-product/qualify_composite.py`, `full-product/qualification.json` |
| Complete candidate captures | `full-product/run/` |
| Windows handoff cases | `windows-handoff/acceptance-88.json` |
| Chrome snapshot helper, not injected CSS | `windows-handoff/chrome-snapshot.js` |
| Artifact verification without execution | `windows-handoff/verify_artifact_zip.py` |

The shared Marionette transport dependency is also committed at
`docs/qa/linux-22/package-pwa/pwa_ui_probe.py` (class Marionette); the
Linux probe imports its original Cloud absolute path, which must be mapped to
the checked-out QA helper in a local adapter. Python3.11+/Pillow are the screenshot
and pixel-analysis dependencies.

The Linux probe includes Xvfb/DBus/sysroot/process handling and is **not a
ready-to-run Windows launcher**. The local native Windows harness should reuse
its Hub/tabbrowser actions, case list and measurement logic, while using native
Windows executable/profile/process/screen capture. The strict Linux comparator
also pins Linux paths/intake and is not a Windows oracle. Record any adapter
changes and actual Windows environment alongside the output; do not relabel
Linux captures. Snapshot helper is a Marionette chrome-context function body,
with closebuttons/fonts/window/animation fields added for matched controls.
It performs observation only, and must not replace styles or product resources.

## Acceptance contract for the local native Windows run

`acceptance-88.json` enumerates all88 unique actual states: four designs
(Proton, Lepton, Firefox UI Fix/protonfix, Fluerial), horizontal and actual
expanded verticalMode, eleven states per design/orientation. The three roles
are genuine blue/orange ContextualIdentityService containers and a normal tab.
Selection/multiselection must be genuine tabbrowser state; hover must be real
pointer state, asserted from actual flags. Synthetic input is acceptable when
reported as such. A pref value alone does not establish verticalMode.

1. Authenticate run/source/artifact digest before installation or extraction.
   Use a dedicated fresh profile and clean product tree. Keep shipped sandbox,
   OS protections and validation enabled; record their effective state.
   Record Windows version, native/product versions and BuildIDs separately,
   theme, density, display scale/DPR and physical/virtual graphics environment.
2. Use actual Hub design/orientation controls and measure shipped defaults.
   The directly comparable default-gate scope is container enabled,
   photon_like_contextline=false, as_titlebar=false and ordinary default
   theme/density. If native Windows defaults differ, retain and report the
   actual stock test; a separate changed-configuration test must be labeled
   explicitly and cannot stand in for stock/original acceptance. Record
   effective gate values rather than assuming.
3. All176 blue/orange indicator ROIs require positive correct-identity-color
   pixels and zero opposite-identity pixels. For Lepton/protonfix measure the
   intended `.tab-content::before` bottom border, not hidden native contextline.
   For Proton/Fluerial measure actual `.tab-context-line` pixels. Require
   positive dimensions/inside-tab geometry. Native contextline display:none is
   not by itself a defect when a valid replacement paints the indicator.
4. Sample pixel centers inside the actual fractional element rectangle. Convert
   CSS coordinates to captured screen pixels using actual Windows origin/DPR;
   verify coordinate alignment. Do not copy Linux's201x2/2x10 dimensions or
   screen origin into Windows expectations. Retain outer ROI counts as
   diagnostic data. The corrected method still detects50 missing Linux
   baseline indicators; never use unrelated whole-tab blue selection borders
   as proof that an orange/blue container indicator exists.
5. Every normal tab must have zero identity-colored pixels. Compare native
   Windows baseline/candidate normal and Proton controls with matching window
   geometry, closebuttons, fonts, label, selected/multiselected/hover state,
   animations and effective gates. Wait for fonts/frames/stable layout and
   record the states. Preserve differences; do not require equality to Linux
   font metrics. Include genuine window-resize/hover controls if a mismatch
   appears. InspectorUtils.getCSSStyleRules was unavailable on Linux; those
   TypeErrors are retained and not counted as successful matched-rule probes.
6. Save JSON and actual chrome screenshots for each unique case, interaction
   assertions, full regular-file hashes/tree entries/symlink targets before and
   after, executable/profile identity, normal unforced quit and owned-process
   cleanup. Keep fresh profile/HOME and credentials out of committed evidence.

Baseline choice must be recorded. Original report is Windows12.20.2/native158;
the candidate is current native159. A same-current159 Windows baseline tests
regression and candidate behavior but does not itself qualify the old-version
condition. An official old Windows release, if available locally, is a separate
version comparison. No additional Cloud dispatch is requested for baselines.

## Limits retained

Linux main captures: baseline15988 + official15840 + candidate15988 =216;
normal supplemental captures9 per product =18, recorded separately. Candidate
indicators176/176; source fixtures before-fail/after-pass; standard CI284/0.
Original strict style comparison remains **1534pass/1fail**, because one
historical normal snapshot omitted its closebuttons condition. Matched fresh
normal9/9 pairs do not erase that observation or prove its historical cause.
Fluerial ordinary expanded-vertical selection was already visible and that
part of the original claim is unreproduced.

Original Windows execution, highDPI, physical GPU/input, actual collapsed
vertical and other gates/themes remain unqualified until actually measured.
The unsigned artifact does not qualify signing/release delivery. Original seven
issue matrices, KAN24 and Linux22 denominator remain unchanged. No merge,
release/public distribution or Done transition.

## Authenticated final Windows installer

[Artifact13459735586](https://github.com/Floorp-Projects/Floorp/actions/runs/42120232936/artifacts/13459735586),
`noraneko-windows-x86_64-unsigned-installer`, ZIP116708306 bytes.
Downloaded ZIP SHA256:
`67df130a88bf1f201621e8f0a1590c59cc9f18aae640350a53ed3bf1c2b2a933`
matches the GitHub artifact API. The single final installer member is
`floorp-windows-x86_64.installer.exe`,116731728 bytes, SHA256:
`e125dd8cdc62df753f225bb12b3fa332627220ca24f26939936c4c6031efb2a5`.
Both hashes were calculated from actual downloaded bytes in memory. No EXE was
executed, no product resources were substituted, and no installer/cache binary
is committed to the QA branch. `installer-verification.json` records the
source/run binding and hashes; `installer-artifact-api.json` contains the
authenticated artifact metadata.

Local retrieval (extracts the Actions ZIP, does not launch the installer):

```powershell
gh run download 42120232936 -R Floorp-Projects/Floorp -n noraneko-windows-x86_64-unsigned-installer -D kan26-windows-installer
Get-FileHash .\kan26-windows-installer\floorp-windows-x86_64.installer.exe -Algorithm SHA256
```

The EXE hash must equal the full digest above before local use. If downloading
the raw Actions ZIP instead, verify both API metadata and ZIP/member digests:

```text
python verify_artifact_zip.py downloaded-artifact.zip installer-artifact-api.json --out local-installer-verification.json
```

The full workflow completed successfully, attempt1, with main job127570368936
successful and macOS finalizers skipped. Both Windows signing steps were skipped
as requested. `source-log-verification.json` records source checkout, verified
Runtime identity/digest and final artifact log lines. Published `package-job.log`
redacts one contact address and one transient artifact-transport URL; both raw
and published SHA256 values are recorded, and unredacted raw-local is excluded
from shared QA. Product/source/digest evidence lines are unchanged.

Latest subject recheck confirms3048 head/base and canonical3041 still match the
pinned values, both Draft/unmerged, and exactly one Windows package run exists
at this head. No duplicate dispatch, retry, release or native Windows execution.
