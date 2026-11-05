# KAN-48 / GitHub #2800 — actual nested KDE Wayland probe

2026-11-05. **Partial product verification; original wrong-Floorp-taskbar-icon symptom remains unqualified because the nested Plasma task model is not listing the actual running PWA.** Do not classify the infrastructure's generic pinned launcher icons as the reported product defect or call the original issue not reproducible.

## Subjects and execution

- Floorp main/tag source `984509d2053be7bae9027133bda7259cd706b208`; actual unmodified official Linux product 12.21.1 / Gecko159.0.1 / BuildID20261103153232.
- Debian13 non-root cloud host, Xvfb X11 outer screen `:88`; actual nested **KWin6.3.6 Wayland / Qt6.8.2**, Plasma desktop and its isolated session bus. KWin is operating in Wayland mode, as measured in `../kwin-support.txt`. This is not the reporter's CachyOS/KDE6.7.5/pacman package.
- Dedicated `KAN-48/kde-profile`, port2855. PWA creation and shutdown used synthetic Marionette element clicks and in-app quit. No physical-input claim. Native Wayland was requested with `MOZ_ENABLE_WAYLAND=1` and the protocol log records real `xdg_toplevel.set_app_id` calls.
- `security.sandbox.content.level=6`, `MOZ_DISABLE_CONTENT_SANDBOX` absent, normal session bus retained. No experiment was forced or security preference lowered. Explicit remote system-access automation opt-in was set only for this dedicated product profile.

## Executed product checks

`pwa_ui_probe.py --port2855 --fixture-port8775 --out KAN-48/ui-create` completed successfully. It created red Alpha through the actual URL-bar UI and blue Beta through the actual application menu, persisted them in the real product SSB store, generated desktop entries and PNG icons, and opened the PWA windows. `ui-create/result.json` and two product screenshots retain the actual UI results.

With the browser already running, KWin observed resourceClass/desktopFileName `floorp` for both PWA windows (`ui-created-windows.json`). This is the independent launch-order/class condition covered by KAN-49; it does not establish #2800's matching-app_id-but-wrong-icon condition.

Then that dedicated product was normally quit, the same local fixture server was restarted, and Alpha was cold-started using the generated launcher's actual executable, `--name floorp-66d7e1e4-333f-4173-ad18-caf95a7fe6a2`, exact profile, and `--start-ssb {66d7e1e4-333f-4173-ad18-caf95a7fe6a2}`. This cold run produced:

| Signal | Actual measured value |
|---|---|
| Wayland protocol app_id | `floorp-66d7e1e4-333f-4173-ad18-caf95a7fe6a2` |
| KWin resourceClass | same unique value |
| KWin desktopFile / desktopFileName | same unique value |
| KWin resourceName | `floorp` |
| Generated desktop StartupWMClass | same unique value |
| Generated desktop Icon | existing absolute Alpha PNG path,128×128 RGBA, red `(220,40,50,255)` |
| KWin icon descriptor | QIcon with available size128×128, cache key recorded |
| Product taskbar experiment | false; log says `PWA taskbar integration disabled by A/B test, skipping OS integration` |

`kwin-alpha-info.txt`, `cold-alpha-windows.json`, `cold-alpha-product.json` and `alpha-cold-wayland.log` retain these observations. **The QIcon descriptor and size do not authenticate its pixel content.** No claim is made that the KWin icon is definitely Alpha, Floorp, or Firefox from the descriptor alone. Product chrome visibly uses Alpha's red fixture icon, but that is not proof about taskbar rendering.

## Concrete desktop capability gap

The visible nested Plasma panel initially showed two generic paper launcher icons. Reading the actual `org.kde.plasma.icontasks` configuration in `plasma-layout.txt` established that these are pinned `systemsettings.desktop` and `org.kde.discover.desktop`; those apps are absent from the minimal isolated desktop. They are **not** the test PWA's icons.

The PWA is present in KWin's real Wayland window list and has `skipTaskbar:false`, but it is not represented in that Plasma taskbar. A stale/missing applications-menu condition was separately found: `applications.menu` did not exist, while the installed file is `plasma-applications.menu`. Running normal `kbuildsycoca6 --noincremental` with `XDG_MENU_PREFIX=plasma-` fixed that menu-cache error (`kbuildsycoca-plasma-prefix.log`) without altering product behavior, but did not recover PWA taskbar enumeration. This task-model/Wayland integration remains an infrastructure gap. No protection was disabled to expose more compositor interfaces or fabricate a passing taskbar result.

Portal Desktop activation still fails and PipeWire client/context configuration is incomplete, as recorded in the retained nested desktop logs. These are not classified as Floorp bugs. Whole outer-display screenshots also contain a profile-lock modal from another isolated X11 test; KWin window snapshots are the authoritative enumeration of this specific nested desktop.

Follow-up read-only checks ruled out a simple current-desktop/Activity mismatch: KWin's current desktop UUID and current Activity both match the PWA's `getWindowInfo` memberships. The icon-task widget has no saved filtering overrides; installed defaults restrict current desktop/Activity and do not restrict screen, consistent with the matched memberships. The actual running Plasma process uses Qt Wayland and the correct socket, but lacks `XDG_CURRENT_DESKTOP`, `KDE_FULL_SESSION` and `KDE_SESSION_VERSION`. Read-only D-Bus calls with KDE session flags explicitly provided succeed; these flags do not retroactively change the running shell. `desktop-filter-observation.json` and `task-filter-config.json` preserve this distinction. The remaining WaylandTaskModel connection has not been independently introspected or recovered; no protection/interface filter was disabled.

## Result and next conditions

Actual KDE Wayland product/UI/launcher/app_id tests ran. The Linux taskbar experiment is disabled in this real profile and was left unchanged. The correct cold-launch identity was verified. The **original #2800 wrong Floorp icon despite correct association is neither independently reproduced nor cleared** here; its remaining conditions narrow to a functioning Plasma task model/compositor integration, icon-pixel observation, and the reporter's CachyOS/KDE6.7.5/pacman package. Multiple-order class inheritance belongs to KAN-49, whose separate fix is not duplicated here.

No new PR is justified by the partial #2800 evidence. Keep open; do not merge, release or mark Done. Preserve specific original-environment and working-Plasma-task-model labels rather than declaring Linux unavailable.

Reproduction artifacts: `../pwa_ui_probe.py`, `serve_fixture.py`, `window-snapshot.js`, `observe-kde.sh`, actual generated launch command in this report, JSON snapshots, product/Wayland log, three whole-desktop PNGs, and `plasma-layout.txt`. Keep browser HOME/profile data and retrieved `sources.json` out of any QA commit.
