# Delivered-product profile-lock audit

Read-only comparison of authenticated full unsigned Linux packages for PR3032 (`8249e234adfbb3e877e4a20cf5f57d7264d9a447`) and PR3040 (`2deb165d75290ffd4f602ad078512dc42fc3a332`). No product execution, profile API calls, source/product edits, or inventory-gate changes were performed for this audit.

## Findings

- All 27 selected delivered native ELF/shared-library/dependency-list files have identical SHA256. Per-file values are in `delivered-comparison.json`. This includes the actual browser executable and libxul; no evidence attributes the observation to a changed native/XPT helper.
- Neither original authenticated tar archive contains a root `.parentlock`, `parent.lock`, or `lock`, or any lock-like member. Each has 57 archive members. PR3032 tar SHA256 is `cf5e657f87c14a50dc6cfadd63d8e27d475f61d62b0c6b2a657f57db337b7146`; PR3040 is `bafdaf44e310aa22b41a9cb740dff47b07c51ada37978008c47d6952c6564976`.
- Both packaged component manifests and both NRProfileManager actor resources are byte-identical. No separate jar `.xpt` resources were found; this is an inventory comparison, not an XPT interface qualification.
- Compared extracted trees differ only in `application.ini`, `browser/buildid2`, `omni.ja`, `platform.ini`, and newly observed PR3040 root `.parentlock`/`lock`. Existing native files remain unchanged. The two root locks are retained as failed-suite evidence; this audit does not authorize deletion or an allowlist.

## Relevant source paths, without causal attribution

In `/workspace/qa-linux-wayland-browser-fix/browser-features/modules/actors/NRProfileManagerParent.sys.mts:84`, `GetProfiles` enumerates profiles. For a non-current profile, lines89–90 call `profile.lock({})` then `lock.unlock()`. Returned root/local paths appear at lines103–109. The packaged actor is unchanged between the two products. This shows a possible lock API caller, but does not establish that its profile root was the product directory or that it caused the observed files.

Locked Runtime source `8965ffc346d426ccdf919bd18585e4d4a0c21654` in `/workspace/qa-linux-full-build/runtime/toolkit/profile/nsToolkitProfileService.cpp:578` forwards profile lock initialization to the stored root/local directory (lines597–618). Profile registry paths are read/resolved at lines1153–1216. No descriptor/path defect has been established.

In the same Runtime, `toolkit/profile/nsProfileLock.cpp:441` clones the requested profile directory and appends `.parentlock`. Linux additionally creates the legacy `lock` symlink (lines470–498). `Unlock` unlinks the symlink (lines548–550), closes the file descriptor, and explicitly retains the `.parentlock` file (lines561–564). A remaining zero-byte `.parentlock` alone can therefore result from an ordinary completed lock/unlock; the observed symlink still requires trace attribution. This source behavior does not explain why the requested directory would be the product root.

## Remaining verification

The PWA test owner is separately tracing a fresh authenticated extraction with isolated HOME/profile and retaining the original failed inventory result. They reported an ordinary/UI diagnostic with no observed app-directory lock operations and an unchanged product tree; non-determinism/trace effects and the original cause remain unresolved. This audit does not classify the original gate failure as a product regression or an infrastructure failure and does not waive it.
