# Draft3032 exact-head complete production package

2026-11-06 JST. [Standalone package run42068055038](https://github.com/Floorp-Projects/Floorp/actions/runs/42068055038) **completed successfully** at source`8249e234adfbb3e877e4a20cf5f57d7264d9a447`. It ran the existing Linux-x64 / stable-v2-validation / skip-signing=true path. The main package job succeeded and all three Mac signing/finalizer jobs were skipped. No retry, workflow/PR/source change, signing, release or distribution action was performed by this monitoring/intake task.

Actual production frontend build, `mach` artifact-mode application build, after-mach injection, `mach package`, DEB repackage, MAR generation and installer-workspace verification all succeeded. This is the complete application/package pipeline using the locked compatible prebuilt native Runtime; it is not a fresh compile of all Gecko C++/Rust.

## Authenticated package intake

- Final Actions artifact`noraneko-linux-x86_64-installer-unsigned`, ID**13436721142**,108203238 bytes, nonexpired, belongs to this exact run/head.
- Downloaded through the connected GitHub artifact tool, then its temporary download reference was used without printing or preserving the signed URL. Actual downloaded ZIP SHA256 **`fc338fa4cd3307c7a8937b2b99c97c18a1665e23b3b561680662fddfc3b336e5`** exactly matches the API digest and size.
- The ZIP contains exactly`floorp-linux-x86_64.tar.xz`; actual inner tar SHA256 **`cf5e657f87c14a50dc6cfadd63d8e27d475f61d62b0c6b2a657f57db337b7146`**.
-57 tar entries were extracted under the expected`floorp/` root using Python's standard data filter, rejecting escaping paths/links/special files and refusing to overwrite a populated output. Product contents were not edited after extraction.
- Extracted product root:`/workspace/qa-linux-full-build/products/3032/floorp`.
- Complete before-execution regular-file SHA256/symlink inventory:`product-inventory-before.json`.

`product-intake.json` binds run/source/artifact identity, ZIP/tar digests, safe extraction policy, raw application/platform INIs and inventory. `package-api-metadata.json` and `terminal-job-results.json` preserve compact API evidence. `intake_product.py` is the reproducible validation/extraction script. The original ZIP/tar and extracted binaries remain local large artifacts and should not be put in the small QA evidence commit.

## Build identity limitation requiring explicit separation

The selected immutable Runtime producer is run41664957250, source8965ffc346d426ccdf919bd18585e4d4a0c21654, expectedBuildID**20261103153232**. Its authenticated manifest and all protected package preflight checks passed.

The completed application's packaged root`application.ini` and`platform.ini`, however, advertise BuildID**20261105165142**, corresponding to this application/package build. Version is`12.21.1@159.0.1`; the root application.ini also says it is not the runtime-used file unless moved and passed with`-app`. Live`Services.appinfo.appBuildID` and version must therefore be recorded independently by the actual-product test; this intake does not infer them from those INIs.

**Standalone package success does not establish the strict final-release verifier's Runtime/final-BuildID contract.** No INI or expected value was rewritten to make that contract pass, and this task did not execute the separate reusable final-release verifier. This is an explicitly retained release-provenance condition, separate from the complete source/package build and the pending actual PWA regression.

## Timing and infrastructure classification

Live step timings are in`completed-step-timings.json`:

| Step | Duration | Result |
|---|---:|---|
|Production frontend build|31s|success|
|Mach application build|58s|success|
|After-mach injection|2s|success|
|Mach package|135s|success|
|Linux DEB repackage|451s|success|
|Full MAR creation|129s|success|

The long DEB step was not a demonstrated hang or product failure. Its existing workflow extracts the tar, adjusts the top-level packaging layout, recompresses an adjusted xz archive, inspects it and invokes normal`mach repackage deb`/`dpkg-buildpackage`. Prior KAN11 package42061375408 is a Mac job and skipped the DEB step, so it cannot serve as a Linux DEB timing baseline. No cancellation, bypass or blind retry was used.

One monitoring formatter encountered null`steps` on the three skipped Mac jobs after completion. This was only the local status-display formatter; the stored API data was intact. The monitoring read was corrected to accept skipped jobs, and both final job/run conclusions were confirmed from fresh API metadata. It is not a package failure.

## Product execution hand-off

The authenticated untouched product and intake/inventory paths were sent to the PWA verifier agent for actual full-product X11 and nested Wayland UI/cold-launch/legacy-ID comparisons. This report claims **build/package/intake success**, not those subsequent regression results. Their owner records the actual product tests, profile isolation, sandbox/experiment state and before/after file invariance separately. Original Manjaro/KDE/Outlook/Teams taskbar conditions, physical input and strict final release qualification remain outside this intake's proof.

## Actual running application identity (subsequent product probe)

The unmodified completed package actually reports `Services.appinfo.version=159.0.1` and `appBuildID=20261103153232`, matching the authenticated native Runtime, in `x11-product-qa/result.json`. Its root application.ini explicitly says it is not used by the application. Therefore its separately regenerated text metadata is not evidence that the running Runtime identity changed. Preserve both observations; do not infer a product bug or release qualification from the unused INI alone. This task does not publish or run the final multi-platform release-verification wrapper.
