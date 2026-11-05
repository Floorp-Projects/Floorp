# KAN-48: new actual original-site icon condition

**New measurement:** the real VRT MAX site named in original GitHub #2800 was installed through the actual packaged URL-bar UI, then its own unchanged generated desktop launcher was cold-started in a fresh normally configured nested Plasma session. Its real native Wayland identity and KWin association match the launcher. The visible taskbar shows the VRT MAX icon, not Floorp. This narrows the untested site/icon condition; it does not qualify original CachyOS/KDE6.7.5/pacman or declare the issue fixed.

## Immutable product and environment

- Complete main product source `29a3eb23145da90706d8a94441373278521cc737`, package run42093687541, authenticated artifact13446089131. This is the unmodified main product, not PR3032/3040 and not a manually patched product.
- Native159.0.1 / BuildID20261103153232; sandbox6, absent disable variable, Linux TaskbarExperiment=false throughout.
- Actual Debian13.6 / KWin6.3.6 / Qt6.8.2 native nested Wayland over own Xvfb106; dedicated HOME/profile/session D-Bus. The normal existing Plasma desktop Exec is relocated to the actual extracted installation prefix, retaining all original interface declarations. No KWin permission bypass or rule forcing desktop identity.
- This is synthetic real WebDriver input and software/headless desktop behavior, not physical-input/GPU or original OS/package qualification. No account, subscription or media playback was used.

## Network capability recovered without disabling verification

First actual browser attempts to both original sites reached `about:certerror`/nssBadCert. Their actor mismatch on that error page is expected and is not the reported PWA product defect. That attempt quit normally and is retained under attempt1-no-browser-proxy-ca/.

The same URLs returned HTTP200 through the inherited session proxy and existing OS CA bundle. Verified host TLS peer issuers were the configured environment proxy CA. The existing **OS-trusted public** `/usr/local/share/ca-certificates/environment-proxy-ca.crt` subject/issuer OpenAI/openai.com and SHA256 fingerprint `C7:1B:4D:1E:9D:77:75:C5:38:C6:88:AB:10:CE:A9:CE:41:7D:73:12:E8:98:38:F9:D1:5F:F7:32:56:CD:7F:C3` were matched, then added through normal NSS CA trust to a new dedicated test profile only. No host trust store was changed, no arbitrary site certificate trusted, no `acceptInsecureCerts`, certificate exception, TLS bypass, experiment force or proxy bypass. Both real browser HTTPS connections then validated normally. Public CA metadata is recorded; no certificates/database/profile or credential values are published.

## Real site outcomes

| Site | Actual browser result | Qualification |
| --- | --- | --- |
| https://www.vtmgo.be/ | Redirects to DPG Media privacy gate, no manifest in that page | Original site's PWA/icon not tested; no consent/authentication bypass or local mirror substitution |
| https://www.vrt.be/vrtmax/ | Actual page and original manifest load; real URL-bar install stores VRT MAX; normal quit; generated launcher cold-starts actual HTTPS page | Original report's real site/icon condition measured on current Debian/KWin; original wrong-Floorp-icon symptom not reproduced here |

Host manifest requests are separate public resource-availability evidence (`original-site-manifests.json`), never substituted into either browser. Privacy redirect query values are omitted from public result JSON; a raw record stays local and excluded. The VRT cookie prompt and DRM notice remained visible; no consent, DRM activation, login or playback was needed for PWA install/icon testing.

## Native and image measurements

The real generated desktop filename/StartupWMClass, `xdg_toplevel.set_app_id` and KWin resourceClass/desktopFileName all equal `floorp-54e02a83-a477-4acf-b027-2a7eea2c788e`. `native-app-id-excerpt.txt`, `cold-result.json` and the actual root screenshot record this.

Generated icon path uses the ordinary `512x512/apps` location but its **actual PNG dimensions are128x128**, SHA256 `32102d9ddd532621b880e3bc72352b13b45fc3029a079631d344a1faf0ef91de`. No assumption that pathname equals pixel dimensions.

The actual taskbar32x32 crop is x274–305/y722–753. Visually inspected `vrtmax-cold-taskbar.png` clearly shows the VRT MAX logo. Compared with BOX-scaled actual generated icon, mean absolute channel difference0.431640625 and maximum1; it is nearly pixel-identical, not claimed byte/pixel-exact. The same actual crop against known Floorp32 gives mean difference117.61328125 using the task background for transparent reference pixels. `pixel-comparison.json` and `compare_pixels.py` retain the exact comparison. This tests arbitrary real-site image content beyond the earlier opaque Alpha/Beta fixtures, which were not repeated.

Real-site install and cold launch each quit normally exit0/forcedfalse/in_app=true. The earlier certificate-alignment attempt also quit normally. Entire product hashes and generated desktop bytes are unchanged; no browser remains. All tracked owned Xvfb/KWin/Plasma/session-bus processes exited, and X106 socket/lock are absent (`cleanup-final.json`). No unrelated processes were killed.

## Classification and remaining work

No new product defect was established, so no speculative source fix or duplicate PR. PR3032/3040 remain related identity fixes; this current-main cold matching-identity case does **not** prove either fixes the original KDE wrong-icon symptom. KAN-23 cause/symptom equality is also not inferred.

Still needed: original CachyOS/KDE6.7.5/pacman version/site/profile/full-desktop context and its reported correct-association/wrong-Floorp-icon condition; VTM GO's usable original application page beyond the privacy gate; arbitrary original reporter-specific permissions/session state. This adds a successful current Debian/KWin real-VRT-site control, not broad original-environment acceptance. OriginalLinux22 denominator is unchanged. No merge/release/Done, message to external people, account operation or macOS/GPU access.

Replay in a fresh dedicated directory with session.py start --baseline, session.py repair-prefix, probe.py then cold_site.py and compare_pixels.py; stop only that owned session. Scripts use the authenticated product and existing QA sysroot. Exclude home/, profile/, runtime/, raw-local/ and any copied browser state when publishing.
