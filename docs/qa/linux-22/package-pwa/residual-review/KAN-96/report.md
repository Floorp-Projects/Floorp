# KAN-96: new COOP/COEP and genuine container-source measurements

Original actor mismatch was **not reproduced in these newly exercised conditions**. This extends the earlier plain HTTP/webIsolated local fixture, rather than rerunning it. Two real PWA installs were performed with visible packaged URL-bar/install buttons, followed by actual generated desktop-command relaunches. No source fix or duplicate PR is justified by these results; #2680 remains the existing actor fix.

## Authenticated product and scope

Complete latest-main product source29a3eb23145da90706d8a94441373278521cc737, package run42093687541/artifact13446089131, API-authenticated ZIP SHA25605164de082b5b6c1f18ec70155e6ae4eebec14486cb983b1a013660fbf00239d. Reused the already completed KAN24 baseline package without rebuilding or changing resources. This is the existing complete production application/package pipeline with locked prebuilt native Runtime, not fresh Gecko or strict final-release/helper/XPT qualification. Live159.0.1/BuildID20261103153232, Debian13.6, normalDBus, ownedXvfb103/X11, sandbox6. Synthetic WebDriver input, not physical input/KDE/Wayland/GPU qualification.

## New measured conditions

Fixture sends Cross-Origin-Opener-Policy:same-origin, Cross-Origin-Embedder-Policy:require-corp and Cross-Origin-Resource-Policy:same-origin over trustworthy loopback HTTP. Actual content reports crossOriginIsolated=true, isSecureContext=true and SharedArrayBuffer=function. Process identity is measured rather than mocked.

| Case | Source tab remote type / origin | Actor / real UI install | Generated launcher |
| --- | --- | --- | --- |
| COOP Default | webCOOP+COEP=http://127.0.0.1, userContextId0 | NRProgressiveWebApp available; real install writes app and desktop entry | Actual PWA root taskbartab, floorpSsbWindow=true, tab floorpSSB=true; isolated content and actor available |
| COOP from genuine Firefox container | webCOOP+COEP=http://127.0.0.1^userContextId=7; originAttributes.userContextId7 | Actor available; real install writes app and desktop entry | App deliberately uses context0 while container rollout is off; isolated content and actor available |

The source container was created by the genuine ContextualIdentityService, not a seeded SSB record. The shipped PWA container experiment is **false** and ordinary Firefox containers are enabled. It was not forced or changed. Both app records have userContextId0 and both launched apps use context0, consistent with the existing gate. Thus **source-container actor/install coverage is met, but assigned-container PWA qualification remains unavailable under the current non-enrolled rollout**. Do not remove that remaining condition or call the gate a product regression. No experiment enrollment, protection relaxation or app-record edits were used.

`container-installed.png` was visually inspected: it shows the actual container-colored browser URL bar and fixture text reporting isolation=true. JSON snapshots independently record actual origin attributes/process type and actor availability. This is a controlled same-origin isolated PWA fixture, not a real external cross-site/embedded-resource site qualification.

## Execution and retained harness attempts

The initial installation and first launcher attempt are preserved in install-and-first-relaunch.json. Both installs succeeded; first launcher content was isolated, but an obsolete ssbid marker assertion failed. The current source uses taskbartab/floorpSsbWindow/floorpSSB. Corrected the observation to those actual shipped markers and continued **only the existing generated launchers** using --relaunch-only; did not reinstall completed app cases. result.json retains original install steps and references their original evidence, then records both successful verified PWA relaunches.

Earlier dedicated attempts are retained separately, all procedure errors rather than product bugs: outdated ContextualIdentityService resource URI (actual shipped URI is moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs), content probe remaining on original blank tab, GetWindowHandles list-shape handling, and chrome-context window handles versus content-context tab handles. The final method uses standard content GetWindowHandles/SwitchToWindow, grounded in Runtime remote/marionette/driver.sys.mjs. No security setting changed to correct these observations.

Eight browser process lifetimes across preserved attempts, install and relaunch continuation exited normally/forced=false/exit0. The qualified install+relaunch sequence has four normal quits including the first marker-only observation attempt; final continuation has two. All observed product inventories are unchanged. Owned browser/display processes0 and display103 socket absent. No source/Jira/sharedmatrix/PR mutation was performed by this subtask.

## Remaining and next executable work

Retain original CachyOS/Arch packages, original safe-mode YouTube Music behavior, real external cross-site content/embedded-resource policies, complete original desktop/permissions and physical input. Controlled COOP/COEP and genuine source-container actor paths are now covered. Assigned-container apps require genuine rollout enrollment; no forced enrollment is appropriate. A next independent Cloud measurement could use actual Troubleshoot Mode with this controlled isolated fixture, keeping the original external music site as a distinct remaining condition. Original Linux22 denominator remains unchanged.

Replay from a fresh dedicated output directory by copying probe.py there and running `dbus-run-session -- python3 probe.py`; the corrected script runs both installs and relaunches in one sequence. Preserve the existing authenticated product path or adapt PRODUCT to another separately authenticated product and record its source. Do not publish run/profile, run/home or run/runtime contents. Small JSON/log/screenshot/script evidence is suitable for the dedicated QA branch.
