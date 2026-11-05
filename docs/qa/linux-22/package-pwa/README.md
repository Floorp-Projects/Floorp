# Linux stabilization Sprint — package/PWA owner A

Baseline source: `984509d2053be7bae9027133bda7259cd706b208` (Floorp12.21.1 / Gecko159.0.1).
Independent QA branch: `codex/qa-linux-package-pwa`. Machine-readable index: `matrix.json`; actual capability measurements: `environment.json` and `environment-probe.log`.

All seven assigned issues received actual product probes. The original Bazzite/Bluefin/CachyOS/Manjaro conditions were not silently replaced by Debian. Xvfb/X11 and actual nested KWin Wayland are separated. Automation is synthetic, not physical input. Product content sandbox preferences and language-pack signature checks were retained; portal/system-bus and nested namespace limitations are explicit in reports.

- KAN-8: official Flatpak profile switch reproduced; exact existing PR198 helper passes six normal-shutdown comparisons and 9 unit tests. Pinned manifest full candidate build/export/install and three normal permission cycles also pass; Bazzite and signed distribution qualification remain.
- KAN-17: four official Floorp/Firefox font-present Flatpak comparisons render Japanese; original Bluefin divergence remains unresolved.
- KAN-23: signed-pack/theme wrong icon reproduced; Draft3030, generated production manifest verified, real-product locale comparisons plus repository assertion-harness matrix.
- KAN-48: focused follow-up identifies and resolves the Plasma task-model prefix-registration cause; official cold PWA taskbar icons match generated PNGs in all1024 pixels. Original CachyOS/KDE6.7/site conditions remain.
- KAN-49: current shared-class defect reproduced and candidate improves three actual launch orders; original first-PWA inheritance wording and full Manjaro/KDE qualification remain separate. Draft3032.
- KAN-89: focused official Flatpak/KDE X11 three-locale main-client probes complete: _NET_WM_ICON absent; taskbar Floorp reference matches within one channel level; Alt+Tab remains genericX. Original Manjaro association and reported Firefox-logo cause remain unqualified; no duplicate/verified3030fix. Related3030 is not a confirmed duplicate disposition.
- KAN-96: actual current product URL-bar/menu creation and webIsolated actor work; original CachyOS/safe-mode/external-site matrix remains.

## Reproduction prerequisites

Scripts preserve the task's absolute `/workspace/qa-linux-22/package-pwa` fixture paths. On another machine, create that dedicated layout or adapt path constants before use. Never point them at a personal HOME/profile. The tracked files exclude browser databases, credentials, sysroot/Flatpak binaries and account metadata. Product-copy scripts require a NEW destination and retain original files; their results are integration comparisons, not full source builds or signed packages.

The task obtained Debian packages in a dedicated container then extracted them into a private sysroot: xvfb, xauth, x11-utils, xdotool, dbus-x11, flatpak, curl, python3, KDE KWin/Plasma and portal dependencies, Noto CJK. The host already has Noto CJK exposed to Flatpak. `environment.json` describes actual capabilities, including the absent full system service bus. Normal session D-Bus was used for successful product launches. `kde-exec.py` contains this session's KWin PID selector; replace it with the new owned compositor PID when reproducing.

`pwa_ui_probe.py` provides the raw Marionette client and local fixtures. Successful controllers verify profile ownership before later control/quit; supply unique ports per browser. Earlier setup failures are distinguished in reports. KAN-8 additionally needs the pinned PR198 helper/tests included below `KAN-8/flathub/`; no complete candidate Flatpak build is implied. Branding scripts obtain normal signed Mozilla159.0.1 XPI files and the official Firefox reference image; no signature preference is disabled.

## CI and review limits

`ci/pr3030/summary.json`, final-head `ci/pr3032-final/summary.json`, and historical `ci/pr3032/summary.json` authenticate downloaded Ubuntu artifacts against GitHub's SHA256 digest and include standard runner excerpts. PR3030's standard CI records282 modules passed and PR3032 records281. Both include their added runAllTests module and 8 downloaded Firefox files/16 tasks passed. This does not establish active signed language packs in CI; that condition is covered by the separate actual-product matrix, not inferred from en-US success. PR3032's first Windows HTTP504 remains recorded; same-head retry succeeded. Both PRs remain Draft. Final PR3032 head8249e234 also passes standard CI; its initial Mac dependency connection failure and successful same-head retry are recorded separately.

Raw logs remain in the local dedicated QA directory. Published text log copies normalize line endings/trailing whitespace and replace KWin's repeated equals section dividers to avoid misleading conflict-marker checks; no observations are altered. Small reproducible scripts, JSON, images, generated manifest and reports are published. Full downloaded CI ZIPs remain at their GitHub artifact IDs and locally, with digests recorded.

No merges, releases or Done transitions. No messages to other chats. No user action is currently required for the completed checks; remaining original-desktop conditions are listed explicitly per issue.

Full KAN-8 package reproduction requires cloning the full canonical Flathub repository into KAN-8/flathub and fetching pull/198/head pinned to d9aa24d58fe633bf765721f95b4ac13132ee397e before running full-build-exec.py. The published flathub subtree contains only the helper/unit-test subset; it is not a complete source checkout. See KAN-8/full-package-report.md for unmodified-manifest source/deployed hashes, unsigned-test-package scope, FUSE/AppStream dependency-path recovery and nine actual normal-exit product runs.

## Focused KAN-48/89 follow-up (2026-11-06 JST)

Read each issue followup/report.md and matrix.json. KAN-48 now has a standard KWrite before/after control, actual KWin denial/authorization and Plasma window-management binding, unchanged original interface grants with only desktop Exec prefix repaired, and actual rendered official/candidate PWA icons. KAN-89 now has authoritative client-list/WM_STATE identification and en-US/ja/es-CL known-theme/known-PNG taskbar/AltTab measurements. The main-window probe gap is resolved; the remaining generic AltTab/empty KWin desktopFileName and original Manjaro/package context are explicit. No product code or PR3030/3032 content changed. Jira10320/10321. All other issue results remain as previously recorded.
