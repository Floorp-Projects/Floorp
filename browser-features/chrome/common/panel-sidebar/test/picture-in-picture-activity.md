# Embedded PiP AppWindow activity

A panel's content browser belongs to an embedded chrome document. Its browsing
context is not an AppWindow. Gecko's `forceAppWindowActive` setter accepts only a
top-level chrome context. Using the embedded document throws before PiP activity
registration and again during player unload, leaving its tab indicator and
player count behind. Normal tabs do not hit this path.

Resolve the containing `topChromeWindow` for AppWindow counts, activity and
window/selection listeners. Normal and panel players then share one reference
count; either opening order and closing an ordinary-tab player while a panel
player remains preserve activity. This is unrelated to the old missing-gBrowser
blank player already addressed by #2699.

The browser-colocated regression creates real AppWindows and embedded chrome
browsers, calls the actual PiP activity methods and checks the native activity
setter and both close orders. It uses no mocked browsing contexts or setters.
The same two cases throw `NotAllowedError` on the original Runtime and pass after
the patch. The host parity test protects the source/extracted resource mapping.

A separate actual-product QA regression uses decoded VP8/WebVTT media, native
PiP menu and close controls, an EWMH-verified Openbox session and real window
minimization. Original code loses activity while a panel player remains and
leaves its indicator/count after the last close. The diagnostic product copy
changes only the named JavaScript archive member; both opening orders pass.
WebVTT captions continued in both baseline and candidate in that fixture, so
caption freezing is not claimed. Native GPU occlusion and other OS desktops
remain unverified. This is not a complete production build.

[QA report and raw observations](https://github.com/Floorp-Projects/Floorp/tree/qa/linux-c-integration/qa-linux-extra/KAN-50/activity)
