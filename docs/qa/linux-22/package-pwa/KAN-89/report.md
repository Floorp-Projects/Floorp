# KAN-89 — current focused official Flatpak/KDE X11 result

Accurate actual-client probes now completed in en-US/ja/es-CL: main _NET_WM_ICON absent, normal sandbox6/exit0. Final Plasma taskbar matches Floorp reference within one channel level after normal desktop Exec-prefix correction; Alt+Tab remains the same generic X image across locales. The reported Firefox logo is not reproduced, but complete expected display is not qualified due unresolved KWin desktop association and original Manjaro/KDE6.7/host Flatpak conditions. No duplicate or verified PR3030 fix is established. Jira10321. See [focused report](followup/report.md), actual client properties and reference/screenshot comparisons. No product/PR code changes.

## Historical earlier probe — main-window measurement gap superseded

# KAN-89 / GitHub #2713 — Flatpak branding path, KDE/Xorg condition remains

Source984509d2053be7bae9027133bda7259cd706b208, official Flatpak12.21.1/Gecko159.0.1 app ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621/runtime26.08. Actual Debian13.6/Xvfb X11 runs used a new dedicated Flatpak HOME/profile and normal isolated session D-Bus; sandbox preference6 and signature requirements retained.

The actual Flatpak changed effective legacy brandShortName from Floorp in en-US to Firefox with signed active es-CL pack. Both launches normally exited0. This establishes the vulnerable branding resolution path in the package, not its original KDE task-switching symptom. Main X11 window was present in root-tree observations, but current Flatpak `_NET_WM_ICON` collection did not return a valid main icon property. A small leader window with a different WM_CLASS and namespace PID made initial window matching unsuitable. This is a measurement gap, not a passing icon result or absent product bug.

KAN-23 separately reproduced the actual wrong icon in the official native tarball and its dedicated GTK theme, with pixel equality against Firefox, and has Draft3030. Do not declare KAN-89 a confirmed duplicate solely from this path overlap. The nested KDE session recovered by this task operates in Wayland, not original KDE/Xorg, and its task model is incomplete as documented in KAN-48. Original Manjaro/KDE6.7.4/Xorg/Flatpak task switching, theme/locale and actual Alt+Tab icon pixels remain.

Evidence: branding/flatpak_run.py, branding/flatpak-en-US-x11.json, branding/flatpak-es-CL-x11.json, branding/flatpak-*-tree.txt, browser/probe logs, and branding/report.md. Related candidate Draft3030 is not a declaration of KAN89 qualification. No duplicate PR, merge, release or Done transition.
