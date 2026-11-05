# KAN-48 / #2800 — recovered Plasma task model and measured PWA icons

Classification: **the previous desktop capability gap is resolved; the original correct-app_id/wrong-Floorp-icon symptom is not reproduced in this current Debian/KWin condition.** This does not qualify the reporter's CachyOS/KDE6.7.5/pacman package, and does not establish that the original issue is already fixed.

## Subjects and normal session

Floorp source/tag `984509d2053be7bae9027133bda7259cd706b208`, unchanged official12.21.1 / Gecko159.0.1 archive SHA256 `7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613`. Actual Debian13.6, KWin6.3.6 / Qt6.8.2 native nested Wayland, outer Xvfb`:91`. Standard KWrite25.04.3 is the control app. All files/HOME/runtime/session bus are dedicated to this follow-up. `subject.json` pins binaries and reference sources.

The session has ordinary KDE session flags, XDG menu prefix, and D-Bus activation executables relocated to their actual private installation prefix. ActivityManager, kded6, plasmashell and KWin names are present on the same session bus (`dbus-names.txt`). No full system bus or hardware/PipeWire desktop is claimed. KWin selected its supported QPainter fallback automatically; no compositor or security bypass was requested. Product controllers are synthetic Marionette/CLI, not physical input. Successful product probes retain sandbox preference6, absent disable environment, experiment=false and normal in-app quit.

## Cause of the earlier missing task entries

The packaged `org.kde.plasmashell.desktop` declares `Exec=/usr/bin/plasmashell`, while the actual executable is `/workspace/qa-linux-22/package-pwa/sysroot/usr/bin/plasmashell`. The original declaration of `X-KDE-Wayland-Interfaces` already includes `org_kde_plasma_window_management`.

KWin's observed log explicitly rejects that interface for the actual executable: `not in X-KDE-Wayland-Interfaces`. Its versioned `utils/serviceutils.h` explains the executable/desktop-Exec canonical-path match, and `wayland_server.cpp` enforces the original interface declarations. This is not inferred just from a missing taskbar icon.

Before repair, the standard KWrite window is present in KWin with a real caption/PID/UUID but absent from Plasma's task list, just as the earlier PWA was. `standard-before.png` and `window-snapshots.json` record the contrast. The control app did not change between observations.

The normal prefix-install correction copies that existing desktop entry into the dedicated user's XDG application directory and changes **only Exec** to the actual installed executable. Its interface declarations are byte-for-byte unchanged; no interface is added. `kbuildsycoca6 --noincremental` refreshes standard service metadata, then plasmashell is normally replaced. `prefix-repair.json` records hashes and verifies the only-line change. `KWIN_WAYLAND_NO_PERMISSION_CHECKS` is absent and asserted absent by the session controller; no permission filter is disabled.

After correction, KWin logs `authorized ... plasmashell ... org_kde_plasma_window_management`, Plasma binds that protocol and receives the same KWrite window UUID, and the same standard window appears as a taskbar entry (`standard-after.png`). The client protocol excerpt records the window and icon requests. This identifies and resolves the infrastructure cause rather than treating generic pinned icons as product defects.

## Actual original-product PWA icon measurement

The unchanged official product creates Alpha (opaque red) through the URL-bar UI and Beta (opaque blue) through the application menu (`ui-create/result.json`). Browser-first execution still gives shared `floorp` identities and a grouped generic Wayland fallback in this tarball test; this is the distinct KAN-49 condition. The native archive is not installed as a host distro Floorp desktop application, so that generic main-browser fallback is not counted as the original wrong-Floorp-logo symptom.

After normal quit, each actual generated launcher is cold-started independently, preserving its executable, `--name`, profile and `--start-ssb`. The dedicated fixture remains served for the entire probe. Both runs verify exact profile ownership and normal exit0. Their actual Wayland app_id, KWin resourceClass/desktopFileName and generated StartupWMClass agree.

| Cold app | Rendered taskbar icon | Exact image result |
|---|---|---|
| Alpha | red32×32, bbox326,722–357,753 | all1024 RGBA pixels match its generated PWA PNG; not equal to known Floorp32 PNG |
| Beta | blue32×32, same task slot | all1024 RGBA pixels match its generated PWA PNG; not equal to known Floorp32 PNG |

`cold-icon-results.json`, `alpha-cold-taskbar.png`, `beta-cold-taskbar.png`, actual generated launcher/icon hashes and KWin snapshots retain the measurements. Screenshots were visually inspected. The pixel assertion concerns the actual taskbar region, not the product's URL-bar site icon. The public fixture icons are intentionally opaque and flat, making exact rendered-pixel comparison unambiguous; arbitrary site icon formats remain a separate condition.

## Relationship to existing PR3032

The existing isolated product copy at PR3032 head `8249e234adfbb3e877e4a20cf5f57d7264d9a447` was also exercised in this now-working session. Actual browser-first UI creation produces two distinct PWA app_id/resourceClass/desktopFile identities and separate red and blue task entries, each with1024 exact expected-color pixels. `candidate-taskbar-results.json` and `candidate-ui-taskbar.png` record this result. The ordinary browser retains its generic `floorp` task.

This is additional evidence for the shared-class/grouping fix already tracked as KAN-49. It does not prove PR3032 fixes #2800's separate matching-identity/wrong-icon report, because the unchanged official product already displays correct icons in the two matching-identity cold runs. No product code or PR3030/3032 content was changed in this follow-up.

## Remaining and reproduction

Remaining: original CachyOS/KDE6.7.5/pacman package, reporter sites/manifests/icon formats, full original desktop service/hardware context. Do not close, merge, release or mark Done; no new product fix is justified by the current #2800 measurements.

`session.py start --baseline` creates the normal dedicated session and retains its supervisor; it needs the earlier task sysroot and the added standard kwrite/wayland-utils packages. With the control app open, capture baseline, run `session.py repair-prefix`, and inspect KWin/Plasma authorization excerpts. `pwa_ui_probe.py` creates actual PWA fixtures; `cold_icons.py` runs their unchanged desktop commands. `session.py exec ...` keeps normal session/environment selection. Use fresh dedicated paths or stop the old owned session first. Do not use a personal HOME. Initial controller-lifetime/stale-lock setup errors and unsupported KWrite --new were corrected before the recorded standard-app/product comparisons; they are not product results. Raw large protocol logs stay local; selected exact excerpts are published.
