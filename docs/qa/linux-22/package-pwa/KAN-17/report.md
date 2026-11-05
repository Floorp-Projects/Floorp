# KAN-17 / #2968 — actual package/runtime comparison

Classification: not reproduced in this font-enabled Debian/X11 condition; Bluefin reporter condition remains untested. No code fix justified.

Source main/tag 984509d2053be7bae9027133bda7259cd706b208. Official Floorp Flatpak12.21.1/Gecko159.0.1 app ab3c23d5f3a32863918263c197b483e37d9ed78a6bb9d00a1657e31dc5e04621 runtime26.08; official Firefox159.0.1 app df51b8005c4cd8f1e185d98ec86d15151ac35a453413e4629c2e46691015b207 runtime25.08.

Debian13.6/Linux6.18.57, Xvfb X11, Flatpak1.16.6; isolated per-run HOME/profile/cache/config with a shared dedicated user installation. Both actual browsers used normal isolated session D-Bus, synthetic Marionette automation, sandbox pref6, no sandbox-disable env. Explicit system-access test opt-in was required. Portal Desktop service was absent; nested browser namespace clone emitted EPERM, with no bypass applied. These limitations are retained, not treated as a product failure or a complete sandbox audit.

Executed font_compare.py: Floorp and Firefox, each with default host fonts and with only NotoSansCJK-Regular.ttc additionally copied into that dedicated HOME. All four rendered 日本語の文字 漢字 かな カナ visibly and recorded 414.5px canvas width at 32px sans-serif. All four normally quit, forced=false. PNGs were inspected; width alone is not the rendering verdict. Fontconfig shows Noto CJK in both sandboxes. A separate successful origin probe finds /run/host/fonts/opentype/noto/*.ttc; these correspond to the host's pre-existing /usr/share/fonts/opentype/noto, not the QA sysroot alone. Floorp runtime26.08 selects KR under the test locale while Firefox25.08 selects JP; this did not cause boxes here and is not established as a defect.

This adds a font-present package/runtime comparison to the earlier WSL font-missing tests. It does not establish that Bluefin44 lacks fonts or explain why the reporter's Firefox works. Do not call the original report fixed or unconditionally unreproducible. Remaining: Bluefin44.20261022.1 package/runtime/font visibility and cache, reporter page/profile, browser UI Japanese and Firefox installation format. Original issue already requests those details; no duplicate request sent.

Evidence: font_compare.py; font-results.json; four PNGs and fontconfig/browser logs; font-origin.log. Fixture and profiles contain only local synthetic test text. Browser HOMEs/Flatpak installation are not for publication.
