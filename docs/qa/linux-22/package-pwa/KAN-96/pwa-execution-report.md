# KAN-96 / GitHub #2695 — actual packaged PWA creation

Source: `984509d2053be7bae9027133bda7259cd706b208`.
Unmodified official Floorp `12.21.1@159.0.1`, BuildID `20261103153232`, Debian 13,
Xvfb X11, isolated HOME/profile and session D-Bus. Content sandbox level **6**.
Synthetic Marionette element clicks, not physical mouse or keyboard input.

`pwa_ui_probe.py` successfully ran both real packaged UI creation paths:

| Fixture | Real creation path | Persisted app | Independent PWA window |
| --- | --- | --- | --- |
| QA PWA Alpha | URL-bar page-action, Install Application | yes | yes |
| QA PWA Beta | Application menu, Web Apps, install current page | yes | yes |

Both content tabs used `webIsolated=http://127.0.0.1`. The actual
`NRProgressiveWebApp` actor instantiated successfully in that remote type.
The packaged UI generated two real desktop entries/icons and records in
`native-profile/ssb/ssb.json`; the probe did not inject store data or replace UI.
Subsequent actual generated-launcher restarts of both PWAs also loaded the
fixture content successfully in all three process-start orders (KAN-49 evidence).

The old remote-type mismatch was not reproduced in this tested local HTTP
condition. This supports the existing actor fix candidate, but does not certify
the original CachyOS package, YouTube Music HTTPS origin, safe-mode behavior or
every Fission/container configuration. No default changed, no experiment was
forced and no new duplicate PR is justified by this passing creation test alone.
The observed Linux taskbar experiment false and shared X11 class are separate
issues, recorded under KAN-49/KAN-48.

Evidence: `../pwa-ui-run-3/result.json`, `alpha-after.png`, `beta-after.png`,
`../native-browser-3.log`, reproducible `../pwa_ui_probe.py` and
`../pwa-probe-notes.md`. The snapshot contains actual product/build/profile,
remote type, actor availability, UI geometry, windows, SSB records and console.

Earlier runs `pwa-ui-run`/`pwa-ui-run-2` failed the Firefox 159 automation system
access requirement before executing the UI test. `--remote-allow-system-access`
alone was insufficient in this product; the successful run used its supported
`MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` automation environment flag. This is explicitly
test automation access and does not lower the content sandbox or change the
security policy of a shipped browser. Infrastructure setup failures are retained
separately from this product nonreproduction result.
