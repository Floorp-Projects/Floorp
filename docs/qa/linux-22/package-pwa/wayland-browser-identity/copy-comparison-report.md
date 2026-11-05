# Native Wayland ordinary-browser identity: existing product versus candidate copy

The existing authenticated PR3032 full package reproduces ordinary-browser
inheritance when a PWA starts the process. The new candidate copy gives ordinary
windows the canonical `floorp` identity while preserving each PWA's generated
identity, a real explicit custom `--name`, and an actual private window.

| Actual native Wayland scenario | Existing PR3032 full product | New candidate copy |
| --- | --- | --- |
| browser → PWA | canonical ordinary browser / unique PWA | same, passes |
| PWA → browser | ordinary browser inherits Alpha | ordinary browser `floorp`, PWA unique |
| Alpha → Beta → browser | ordinary browser inherits Alpha | ordinary browser `floorp`, both PWAs unique |
| existing-profile Alpha → browser → Beta | ordinary browser inherits old Alpha id | ordinary browser `floorp`, old PWA ids retained |
| explicit `--name qa-floorp-custom` browser → remote PWA → real `--new-window` | both ordinary windows retain custom, PWA unique | same, passes |
| PWA → real `--private-window about:blank` | actual private window inherits Alpha | actual private window `floorp`, PWA unique |

All six candidate-copy cases satisfy the measured native identity checks.
Baseline browser-first/custom controls pass and the four PWA-first normal/private
conditions fail the intended ordinary-browser identity check. Execution
completion is recorded separately from those product results.

## Product origin and test method

- Existing product: authenticated completed package run42068055038, source
  `8249e234adfbb3e877e4a20cf5f57d7264d9a447`, installer artifact13436721142.
- Candidate: a separate copy of that product with production-generated ESM
  substitutions in omni.ja; native and other product files unchanged. It is
  **not** a full source/package build. `candidate-resource-provenance.json`
  records resource hashes and source diff; `candidate-commit-source-binding.json`
  verifies the three source hashes match committed source
  `2deb165d75290ffd4f602ad078512dc42fc3a332`.
- Debian13, owned display96 and nested KWin6.3.6, native Wayland protocol plus
  KWin resourceClass/desktopFileName at every step. Products run with normal
  dedicated D-Bus sessions, sandbox level6 and taskbar rollout false. No
  enrollment, sandbox setting or compositor protection is disabled.
- Fresh fixtures are installed through actual product UI. The migration fixture
  comes from the earlier actual PR3032 full-package UI profile/home; originals
  retain identical inventories and SSB-store hash. Copies omit transient locks
  and change only the automation port before normal product execution.
- Actual generated launcher `--name`, `--start-ssb` and saved ids/URLs remain
  intact. Only executable/profile argument paths are mechanically relocated for
  copied profiles/products; stored identity and desktop files are not edited.
- Ordinary window titles are marked only for unambiguous native-window mapping,
  after reading real ssbid/taskbartab state. This matters because normal session
  restore can display a fixture page titled “QA PWA Beta” in an ordinary window.
  The harness never assigns a native app_id or windowclass. It observes actual
  protocol, KWin, product attributes and launcher data.
- The private control reads real `PrivateBrowsingUtils.isWindowPrivate` and
  confirms exactly one ordinary private window. This is a genuine CLI private
  window, not a mocked boolean or preference.

The twelve qualification cases completed with **14 normal in-app quits**, no
owned processes remaining, product inventories unchanged and original migration
profile/home unchanged. Input is synthetic UI/CLI; no physical-input claim.

## Retained infrastructure/harness attempts

`existing-3032` failed before product execution because its complete Wayland
socket path exceeded the OS's108-byte AF_UNIX limit. The retry uses a normal
private0700 short runtime directory under /tmp and removes it after cleanup.
This was an infrastructure failure, not the reported product defect.

The first custom control used an ordinary URL CLI request, which correctly
opened a new tab rather than the second browser window required by the control.
All observed custom identities were preserved, but the case was incomplete.
`existing-3032-custom-control` is retained; the qualifying retry uses genuine
`--new-window`. No native identity was manually changed to create a pass.

## Evidence and limits

`copy-comparison-matrix.json` contains the compact results. Each qualifying
folder contains result.json, identity-projection.json, per-step KWin observations,
actual CLI commands and native-protocol.json. Original raw observations are
preserved even where title-based interpretation was corrected.

These results do not authenticate Plasma taskbar icon pixels or establish the
original Manjaro/CachyOS/KDE package combination. New full production package
run42077174018 is a separate qualification stage; its future results must not be
replaced by, or merged into the origin label of, this candidate-copy evidence.
No source edits, push, merge, release or Done transition were performed by this
test harness.
