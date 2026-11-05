# KAN-24: dependent Draft3046, standard CI and complete production products

The concrete startup-sync and stale unrelated-save defects are fixed by the tested dependent Draft [3046](https://github.com/Floorp-Projects/Floorp/pull/3046). Its fixed base is an unmodified mirror of canonical Draft2950 head69d306b11c9e52da9e6270870740d49b1d561659. The author fork/canonical PR is unchanged; this supplement is not an alternative fix or independently mergeable. Source head d058d7001b822f0ef2413b1307c98a463dfdcdb9 changes two production files and two browser test files.

## Separate source and CI streams

| Subject | Source / Runtime | Standard CI | Complete product |
| --- | --- | --- | --- |
| Fixed dependency supplement | d058d700 /158.0.1 |42093984715: success,267modules/0fail | No158 full-product claim |
| Latest main baseline |29a3eb23 /159.0.1 | Existing main, no added-test claim |42093687541 success, artifact13446089131 |
| Latest main+2950+supplement |493d2974 /159.0.1 |42094068516 attempt2: success,284modules/0fail |42094061748 success, artifact13447726956 |

Both standard Ubuntu artifacts were retrieved and authenticated against GitHub API SHA256. The observer module declares7cases and settings module3cases; logs establish successful complete-module execution, not independent per-case success lines. Integration macOS CI initially received no runner due hosted arm64 capacity, then that job alone succeeded on one retry. Successful Ubuntu was not repeated. This CI native check is not original macOS Photon UI testing. See [CI evidence](standard-ci/report.md).

The integration retains main's Runtime lock and workflows unchanged; the four supplemental source/test blobs exactly match the dependent head. It applies the canonical eight-file patch before the supplement. Both full products came from the existing complete production frontend/application/package pipeline, using authenticated compatible prebuilt native Runtime. No product resource was substituted after extraction. This is not a fresh native Gecko compilation or strict signed/final-release/helper/XPT qualification. The unchanged workflow already relaxes empty auxiliary archive checks; this task added no relaxation/helper substitution and does not turn that into a strict qualification pass. See package-workflow-scope.json and [authenticated intake](full-product-intake-summary.json).

## Actual complete-product results

All6 product runs completed. Baseline failures below are expected defect reproductions. Native version159.0.1/BuildID20261103153232, Debian13.6, normalDBus, ownedXvfb102/X11, sandbox6; input was synthetic realWebDriver/shadowbutton clicks, not physical input/KDE/Wayland.

| Test | Complete main baseline | Complete integration |
| --- | --- | --- |
| Natural oldprofile nativefalse/storedvertical, normalquit then coldstartup | Mismatch persists | Storedhorizontal, nativefalse; all tabsblank/noHub |
| Natural oldprofile nativetrue/storedhorizontal, normalquit then coldstartup | Mismatch persists | Storedvertical, nativetrue; all tabsblank/noHub |
| Hub→native, nativeoff/on→Hub | Native→Hub mismatch reproduced | Live bidirectional agreement |
| Settled newwindow | Not repeated in these new baseline runs; earlier separate evidence retained | Both actual windowsvertical |
| Workspace first→second→first, multirow, normalrestart | Workspace not repeated in these new baseline runs; earlier separate evidence retained | Multirow retained with nativeoff |
| Actual shipped settingsmodule stalehorizontal/latestmultirow unrelatedfavicon save | Wrongly overwrites tohorizontal | Retainsmultirow |
| Actual shipped settingsmodule stalemultirow/latesthorizontal unrelatedfavicon save | Wrongly overwrites tomultirow | Retainshorizontal |

The4GUI runs passed all expected controls (6checks each baseline,10each candidate). The shipped-module API comparison is **2fail→2pass**. It dynamically imports the exact module in each product's omni.ja from the real Hub and uses its exported getter/save plus actual SettingsRPC. The stale form comes from that getter; an external real preference writer represents the other window. No replacement source bundle, RPC/Services mock, CSP/sandbox disabling or product mutation. This is not a timing reproduction of two visible windows' pending autosave race. Original full-product actual tabstrip followed the native setting; the original report's claim of a physically stuck vertical strip is not reproduced by these tests.

Ten normal quits across6 successful runs, no forced quit, every product hash unchanged, owned processes0, owned display cleaned. One earlier harness attempt used a contentLocation setter fromaboutblank and was correctly denied; it quit normally and is retained as a navigation-instrumentation failure. StandardWebDriver Navigate resolved it without changing browser protections. See [product report](full-product/report.md), [machine results](full-product/matrix.json), [parent verification](full-product/parent-verification.json) and replay scripts in ui/.

## Remaining scope and coordination

Original macOS Photon is deliberately not accessed while the Mac owner is active. Actualtwo-window pending-save timing and many-tab multirow overflow geometry are not established here. CurrentLinux complete products and standard CI support this supplemental fix; canonical2950 and dependent3046 still require review/integration in dependency order. Neither is merged or Done. OriginalLinux22 denominator and assigned7matrix are byte-unchanged; KAN7 GPU conditions remain untouched. No release/public distribution/signing workflow change or human action request.

Jira KAN-24 comment10403 records this continuation. All small, non-secret evidence is published only on the dedicated QA branch; profiles, migration seeds, products, signed URLs and download archives are excluded.
