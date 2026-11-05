# Supplementary actual X11 native identity smoke

PASS: cold Alpha launcher → ordinary browser remote new-window → Beta launcher retained three native WM_CLASS values: Alpha floorp-9c81f296-e163-468d-bded-591d97ede773, ordinary floorp, Beta floorp-6a449a02-b424-4586-b14a-799c18105ee3. Each real client came from root _NET_CLIENT_LIST; WM_STATE, WM_CLASS, title and PID were captured and matched real browser root attributes. All three clients shared the guarded browser PID125435.

Dedicated Xvfb :98 and KWin X11; own DBus/profile, port2898. Existing QA profile/home were copied; launcher executable and profile arguments relocated while --name/start-ssb IDs stayed intact. No SSB-store rewrite. Original fixture HTTP server was not running, so PWA content showed Problem loading page; this is not a navigation-pass claim. It does not prevent native top-level identity measurement.

Candidate is the source-generated ESM copy from official3032 CI package, not a new full build. Native159.0.1/BuildID20261103153232; sandbox6/no disable environment. Normal unforced Quit, browser exit0; owned processes0; all product hashes unchanged. KWin logged optional missing overview QML/effects/global-accelerator components; managed native client lists and identity properties were available. This is not full Plasma, Wayland, physical input or taskbar/AltTab verification.

result.json and run.py retain observations and reproducible procedure. The script was adapted from the colocated runner; display/kind metadata was corrected after execution to accurately describe this smoke test. No observed native properties were changed.
