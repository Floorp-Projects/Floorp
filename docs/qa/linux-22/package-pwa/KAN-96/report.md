# KAN-96 / GitHub #2695 — actual current Linux PWA UI

Classification: original actor mismatch not reproduced in this current Debian/X11 condition; existing fix #2680 supported by execution, original CachyOS not qualified.

Official Floorp12.21.1/Gecko159.0.1, source984509d2053be7bae9027133bda7259cd706b208, Debian13.6, Xvfb/X11, dedicated HOME/profile, normal isolated D-Bus. Synthetic Marionette clicks in the actual unmodified packaged UI; no seeded app datastore, no experiment forcing, sandbox preference6 and no disable env.

pwa_ui_probe.py created local Alpha via URL-bar #ssbPageAction and local Beta through the application menu. Both saved manifest records and opened PWA windows. remoteType was webIsolated=http://127.0.0.1 and NRProgressiveWebApp actor could be obtained; no reported actor/remote-type mismatch. Generated .desktop/icon files exist. Follow-up pwa_order_probe.py launches those actual generated commands in three orders, yielding both PWA windows plus the main browser each time; normal shutdowns were observed. Linux taskbar experiment was false; window-class behavior is tracked separately as KAN-49.

Initial no-profile-directory and system-access-flag-only attempts failed in the harness. They are retained as infrastructure failures. Successful automation used explicit MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1, port isolation and ProfD identity checks. Early pre-guard order attempts are superseded by the guarded results.

Remaining: original CachyOS/Arch packages, safe-mode YouTube Music and other real external cross-site/COOP/container conditions, complete desktop and physical input. Local fixtures verify HTTP webIsolated actor path and the two product UI entrypoints; they do not exhaust remote types. Existing #2680 is canonical actor fix; no duplicate PR made and no Done transition.

Evidence: ../pwa-ui-run-3/result.json, ../pwa_ui_probe.py, ../pwa-order-run/result.json, ../pwa_order_probe.py.
