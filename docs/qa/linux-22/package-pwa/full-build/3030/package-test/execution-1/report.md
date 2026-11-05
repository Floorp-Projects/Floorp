# PR3030 actual workflow-generated Linux package comparison

**Result: the complete production-path package reproduces KAN-23 before the fix and resolves it after the fix in both Japanese and Spanish. Both product directories remain byte-for-byte unchanged throughout the test.**

Candidate: PR3030 head `e55152c559a141001a375ac77098d141b0db3ca7`, successful package run42067997767, installer artifact13435129152. Downloaded ZIP SHA256 `96ebf4afca0028715039d5154a27fd030e8150cb8117b0249d91eb576411d3fc` matches the authentic GitHub artifact digest. Enclosed tar SHA256 `b138b808c3fc4f4e54ecff0345b47f24660d10ec6351812ba49ac69f70915593`. Files extracted from this tar were individually verified against its file bytes.

Baseline: source `984509d2053be7bae9027133bda7259cd706b208`, successful identical package pipeline run42068377281, artifact13435702404; authenticated ZIP SHA256 `83667970ddcfc2d3f350367f511106ddaf6f52c5ca8ae3069a44870aee22913c`, tar SHA256 `5a087bfd1d385fddb1304b97379a02e780451c07006cc0ffe078f53e21633d82`. Baseline intake was independently performed by root; its authentic intake record is included in provenance.json.

These are unsigned validation **artifact-mode Linux packages produced by the established configure/frontend-build/build/inject/package workflow**, using locked prebuilt Runtime8965ffc346d426ccdf919bd18585e4d4a0c21654. No manifest editing, installed-product substitution or frontend-bundle replacement was performed. This is not a claim of new native recompilation, production signing, publication or release qualification.

| Workflow package | Active locale | Actual legacy brand | Complete new test module | Actual native icon | Native menu |
| --- | --- | --- | --- | --- | --- |
| Baseline984509d2 | ja | Firefox | FAIL as expected | Firefox64, exact RGBA match | Japanese retained |
| Baseline984509d2 | es-CL | Firefox | FAIL as expected | Firefox64, exact RGBA match | Spanish retained |
| PR3030 e55152c5 | ja | Floorp | PASS | Floorp64, exact RGBA match | Japanese retained |
| PR3030 e55152c5 | es-CL | Floorp | PASS | Floorp64, exact RGBA match | Spanish retained |

Each row used a new independent HOME/profile. Each official Mozilla159 language pack was installed via normal AddonManager in en-US, followed by normal quit/restart into the requested locale. Both requestedLocales and actual locale are asserted, along with post-restart active pack/signedState2; a fallback language cannot pass. All eight install/test browser launches quit normally with exit0. Content sandbox remains6 and MOZ_DISABLE_CONTENT_SANDBOX is absent. A deliberate identical GTK fixture theme contains both known Floorp/Firefox64 PNGs; standalone GTK diagnostics confirm both icons resolve. This is a controlled-theme test, not a default-desktop-theme claim.

The complete LegacyBranding.test.mts module plus unchanged repository runTests/assertEquals harness was executed in each actual packaged browser through a focused controller, not the top-level colocated orchestration CLI. Baseline failure is the regression's required negative control, not a baseline product pass. Candidate Japanese labels remain 設定 / 新しいウィンドウ and Spanish labels Ajustes / Nueva ventana. Popup state open, visibility and positive row geometry were asserted; root screenshots include the actual native popup. The validated KWin _NET_CLIENT_LIST client supplied the icon pixels. Native titlebar screenshots visibly change the Firefox logo to Floorp while retaining localization.

`packaged-branding-manifests.json` reads the **actual package archives**: baseline has no legacy-brand override; candidate has exactly the generated override in root `omni.ja`, member `chrome/chrome.manifest`, targeting the existing packaged resource:///chrome/en-US/locale/branding/brand.properties. An initial inspector incorrectly searched only browser/omni.ja; correcting this inspection assumption found the normal generated registration. No product bytes were changed to make that check succeed.

Both actual browsers report Services version/platformVersion159.0.1 and app/platform BuildID20261103153232, matching the authenticated locked Runtime input contract. The root application.ini explicitly declares itself unused by the browser and contains packaging metadata12.21.1@159.0.1 with each package's new build timestamp (candidate20261105165053; baseline20261105165328). Those unused metadata values are recorded separately, not substituted for the live Runtime identity and not treated as a product bug. See authenticated-runtime-contract.json and each result row.

Summary:4/4 expected comparisons, unchanged package-file hash maps before/after, no manual product patch. Dedicated Xvfb/KWin display94 was removed; cleanup.json records no extra owned process and no remaining selected process. No source or PR changes were made during this comparison. Current PR-head revalidation can be performed by root separately; the tested artifact's exact head is authenticated above.

Primary proof paths under this execution-1 directory: summary.json, matrix.json, results.json, provenance.json, authenticated-runtime-contract.json, packaged-branding-manifests.json, package-hashes-before.json/package-hashes-after.json, cleanup.json, *-gtk-theme.txt, *-client-list.txt, *-xprop.txt, *-icon-numeric.txt, *-icon-64x64.png and *-menu-root.png. Parent artifacts/package-provenance.json and installer-metadata.json contain candidate download/intake details; ../compare_full_packages.py is the reproduction controller.

Remaining scope: original Flatpak/KDE KAN-89 is separate; other locales/themes/channels, native recompilation, signing/release and physical input were not qualified. Do not merge, release or mark Done from this result. Exclude *-home/, desktop-home/, runtime/, compiled helper and package archives from public QA evidence; the small JSON/script/log/image records suffice.
