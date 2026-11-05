# KAN-23 standard-runner active-language-pack capability measurement

The remaining top-level standard runner + active signed language-pack condition was actually attempted, but **not satisfied**. This is a test-orchestration capability gap, not a product regression, successful test, or failed reproduction of the branding issue. Existing full-product ja/es-CL icon/branding comparisons and complete test-module API execution were not repeated.

## Measured subjects and sequence

Exact Draft3030 source e55152c559a141001a375ac77098d141b0db3ca7; authenticated unchanged complete PR3030 product from the previously recorded package run42067997767/artifact13435129152, at `/workspace/qa-linux-full-build/products/3030/floorp`. Native159.0.1/BuildID20261103153232, dedicated Debian/Xvfb105 X11/normalDBus, fresh profile, sandbox6. Product hashes before/after are identical.

1. Normally install the existing official ja.xpi through AddonManager, signedState2, request Japanese and normally quit.
2. Start the **unchanged real top-level** `deno task test --near browser-features/modules/modules/i18n/test/LegacyBranding.test.mts --no-autostart --timeout-ms 15000` before browser startup. Its normal fixed test-profile path temporarily resolves to this dedicated profile; owned port-file state is restored afterward.
3. Launch the unmodified complete production browser. Capture live localeja, active signedState2 language pack, executable/profile equality, sandbox6, startupModeproduction, and absent nora.tests.state.
4. The standard host runner discovers exactly one target module, writes its genuine run-id/filter control, then times out after15000ms with `last status: (none)`, exit1. The production browser never emits a browser-test state. Normally quit the browser, preserve logs, remove only owned test-profile link/port selection.

`attempt2/standard-runner.log` proves actual standard orchestration execution; `actual-identity.json`, `standard-control.json`, `result.json`, normal-quit records and product inventories contain the direct evidence. Two launches quit normally exit0/forcedfalse, owned processes0, X105 lock/socket absent. The first harness attempt ran the Deno task in `/workspace` rather than the source directory and ended before creating controls; `attempt1-cwd/` preserves that unrelated instrumentation failure. The retry changes only subprocess cwd.

## Contract explaining the result

`runner-contract.json` pins exact source blobs and records that the host collector only reads `nora.tests.state` from `PATHS.profile_test/prefs.js`. Browser-side test execution is compiled under `import.meta.env.MODE === "test"` and normally imports `http://localhost:5181/loader/test/index.ts`. The complete production product instead reports `nora.startup.mode=production`, follows its production core branch, and contains no loader/test or LegacyBranding.test archive entries. A host `--no-autostart` collector cannot turn a production bootstrap into a test bootstrap merely by setting control prefs.

No test-state fabrication, local replacement source injection, product-archive edits, test-bootstrap preference bypass, signature disabling, sandbox lowering or fake completion was used. Simply increasing the timeout cannot add an absent test bootstrap. Current successful standard CI is still the already authenticated en-US/default-locale condition; active-language-pack standard-runner coverage remains distinct from the previously successful actual-product test-module bundle.

## Specific next Cloud work and limits

To close this runner-specific gap, prepare a **separate normal MODE=test environment** with the standard `feles-build test` path and the same exact source/native lock, install a signed language pack into its isolated test profile, then start the unchanged top-level collector before the browser. That is a different developer-runner build/runtime configuration, not an unmodified complete production package. The current prepared worktree has production preprocessing/dependencies but no `_dist/bin/floorp` test runtime; this bounded experiment did not initialize another large runtime/build merely to relabel the production product.

Other locale/theme/channel coverage and original Manjaro/KDE/Flatpak for KAN-89 remain outstanding. KAN-23's controlled full-product branding fix and existing CI qualification are unchanged. No PR/source/Runtime-lock changes, rerun of old CI/full builds, Jira changes, merge, release or Done transition.

Replay `dbus-run-session -- python3 probe.py <fresh-attempt-name>` in the existing QA dependency layout. This intentionally reports the unsatisfied standard-runner capability; do not treat collector exit1 as a product-test failure. Exclude all `profile/`, `home/`, `runtime/` directories from publication.

## Follow-up: normal MODE=test route measured before activation

The next normal `feles-build test` path was assessed rather than stopping at the production-collector mismatch. The only already installed developer native cache is Runtime157/daily1117, while this source locks Runtime159/daily1179; its matching-marker/INI integrity checks do not permit silently treating the production package or that older cache as the required developer Runtime. The locked159 archive is86,774,264bytes; free workspace at the decision was below600MB, so another staged runtime plus developer outputs would also have little margin. No native download was attempted. This is secondary to the decisive configuration constraint below.

The exact standard source calls `Builder.run("test", ...)`, injects its normal developer bootstrap, and calls `DevEnvManager.setup({allowBrowserHttpLoader:true})`. Its unchanged exported `savePrefs` helper was **actually executed only into an isolated, never-launched recipe directory**. The generated preferences include sandbox content level1, baseline chrome CSP disabled, disallowed privileged HTTPS script loads setfalse, and unrestricted parent JS loadstrue. These are recorded with exact source blob/line hashes and generated-file SHA in `testmode-assessment/generated-settings.json`.

Invoking that unchanged normal developer bootstrap would lower the protections required by this task's retained sandbox6/full-product comparison. It was therefore **not launched**, no live profile was given that generated recipe, and no source/test result was changed to manufacture success. The first generation command lacked the established Deno cache selector and could not resolve an uncached JSR manifest; its log is retained. The same helper using the existing cache then generated the exact recipe successfully, without downloading a Runtime.

This is a policy/configuration-contract constraint, not a product bug or a claim that Linux is unavailable. Existing standard CI success remains evidence for its standard developer harness; it is not retroactively presented as sandbox6/full-product qualification. Full production signed-language-pack comparisons remain separate and successful. Further repeated production timeouts or an unchanged developer launch would not close this particular gap under the retained-protection condition. Any future test-harness hardening is a separate design/scope decision, not a justified product-fix PR here.

Publication: retain `generated-settings.json` and generation logs. Exclude the raw never-launched `testmode-assessment/generated-profile/` recipe directory as well as all ordinary profile/home/runtime directories. Its generated-file SHA is sufficient to bind the reviewed recipe while avoiding wholesale generated profile contents.
