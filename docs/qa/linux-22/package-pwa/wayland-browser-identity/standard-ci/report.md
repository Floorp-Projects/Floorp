# Standard CI evidence: Draft PR3040

SUCCESS: run42077116984 at head2deb165d75290ffd4f602ad078512dc42fc3a332. All five jobs succeeded; Ubuntu x64 executed the combined real-browser suite. 281 modules passed, 0 failed; downloaded Firefox coverage8 files/16 tasks all passed. No retry or code change was performed.

Artifact13438134467 test-logs-ubuntu-24.04 is bound by GitHub workflow metadata to this exact run/head. Downloaded ZIP SHA2561856010f6fc5081dc86292530be029fe05e612e15df5a2034f0e7af183b6d054 matches the published digest. authenticated-metadata.json retains nonsensitive metadata; no signed download URLs or response headers are saved.

The real runner completed SsbCommandLineHandler.test.mts as module231/281 in12864ms. The exact head source registers all four cases, including “Linux PWA startup keeps ordinary browser and PWA identities separate”; its collect-and-throw runner reports module success only after every case resolves. Browser logs additionally show the new Browser Identity Regression PWA being created. The artifact does not provide separate per-case success records; do not describe the module count281 as individual test-case count.

Primary evidence: unpacked/_dist/ci-combined-browser-test.log lines808-809(target module),912-913(summary), and unpacked/_dist/ci-feles-build-test.log contains actual new-test PWA startup. head-test-module.mts preserves the authenticated-head source used to interpret module pass. summary.json is machine-readable.

This is the standard development browser runner against locked Runtime, distinct from a full production candidate package and compositor-native Wayland identity verification.
