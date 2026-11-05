# Draft3040 full production package and authenticated intake

2026-11-06 JST. [Unsigned Linux package run42077174018](https://github.com/Floorp-Projects/Floorp/actions/runs/42077174018) **completed/success** at exact source **`2deb165d75290ffd4f602ad078512dc42fc3a332`**, branch`codex/linux-browser-wayland-identity`. Main job127420377991 succeeded; all three Mac signing/finalizer jobs were skipped. No retry, cancellation, source/workflow/Runtime-lock/helper/XPT change or publication was performed by this monitoring/intake task.

This uses the existing complete Floorp production frontend/application/package pipeline with the locked prebuilt native Runtime. It is not a fresh native Gecko rebuild and makes no CloudB strict helper/XPT or signed-distribution qualification claim. Source delta from3032 base8249e234 is exactly the three intended PWA handler/Linux support/test files; lock and package-workflow blobs are unchanged (`source-subject.json`).

## Completed stages

| Stage | Duration | Result |
|---|---:|---|
|Production frontend build|31s|success|
|Mach artifact-mode application build|60s|success|
|After-mach injection|2s|success|
|Mach package|140s|success|
|DEB repackage|376s|success|
|Full MAR generation|134s|success|
|Installer workspace verification and final uploads|completed|success|

Main job ran17:42:09–17:58:20 UTC. Monitoring used roughly60-second bounded reads. The long DEB/MAR compression stages completed normally; no product or infrastructure failure was inferred from elapsed time and no blind retry was attempted. Exact timestamps and step results are preserved in`completed-step-timings.json` and`terminal-job-results.json`.

## Authenticated immutable subjects

- Final Actions artifact`noraneko-linux-x86_64-installer-unsigned`, ID**13439144608**,108200583 bytes, nonexpired, exact producer run/head association.
- Actual downloaded artifact ZIP SHA256 **`febce14e382da315a7579464f1052768a0e51854a752de8a926ee898db6ce996`**, equal to API digest and byte count.
- ZIP contains exactly`floorp-linux-x86_64.tar.xz`; inner package SHA256 **`bafdaf44e310aa22b41a9cb740dff47b07c51ada37978008c47d6952c6564976`**.
-57 tar entries under the expected`floorp/` root, safely extracted using the standard data filter and a no-overwrite output guard. No resource substitution or other product edit was made during this intake.
- Product root: **`/workspace/qa-linux-full-build/products/3040/floorp`**.
- Full regular-file SHA256/symlink inventory before execution:`product-inventory-before.json`.

`package-api-metadata.json` and`product-intake.json` bind the run/head/digests/layout to these extracted product bytes. `intake_product.py` is the reproducible validator. Download used the connected GitHub artifact tool followed by its temporary file download reference; signed download URLs and credential values were not preserved. Large ZIP/tar/product binaries should remain local, outside the small QA evidence commit.

## Separation from the earlier candidate copy and strict Runtime qualification

The earlier candidate was a3032 product copy with standard generated ESM substitutions. This product is instead the untouched **new complete3040 CI-built package**. `generated-resource-correspondence.json` compares its six relevant ESM additions/replacements/removal with the earlier normal-generated candidate resources; that scoped comparison does not claim whole-package or native-byte equality.

Packaged root application/platform INIs report version`12.21.1@159.0.1` and application/package BuildID**20261105174535**. The selected original Runtime producer's expectedBuildID remains**20261103153232**. These identities are retained separately in`product-intake.json`; live`Services.appinfo.appBuildID` is recorded by the actual-product verifier. Root application.ini contains the normal “not used” note. No INI, expectation, native helper or Runtime lock was changed to fabricate a strict BuildID/helper attestation pass. Standalone package success is not the separate strict final-release verifier or CloudB qualification.

## Actual-product hand-off

The authenticated product root, intake and before-inventory were sent to the PWA agent with an explicit full-package GO for fresh-profile X11/nested Wayland cold-PWA→ordinary-browser identity tests. The agent records execution and after-inventory separately. This intake report claims build/package/authentication success, not its pending GUI results. Original distro/taskbar/physical-input and signed release conditions remain distinct. Draft status/merge/release/Done were not changed.
