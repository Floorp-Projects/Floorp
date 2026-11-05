# KAN-49: ordinary browser identity after cold Linux PWA startup

Draft3040 is a small dependent follow-up to Draft3032. Base3032 head8249e234adfbb3e877e4a20cf5f57d7264d9a447 remains unchanged. New source is2deb165d75290ffd4f602ad078512dc42fc3a332, in an independent worktree/branch. Latest main was separately observed as29a3eb23145da90706d8a94441373278521cc737; it was not silently substituted for the requested comparison base.

## Cause and change

A generated PWA launcher's `--name` sets the process-wide GTK identity. PR3032 already supplies each PWA's explicit windowclass through taskbartabclass, but ordinary browser windows have no override and inherit that process identity. The locked Gecko's existing pre-layout category runs synchronously after taskbar attributes are applied and before initial layout; its native windowclass path sets Wayland application_id. Source references and lifecycle assessment are in source-analysis.md.

Only an initial Linux --start-ssb command registers the process-local category. Its synchronous callback gives an ordinary window without an existing class the canonical remoting name. Existing taskbar/PWA identities and explicit classes are preserved. Browser-first processes, including custom --name and the first remote PWA request, do not register the callback. Old desktop entries/profile IDs need no rewrite. The registration is nonpersistent; no profile migration data, native interface, rollout gate or Runtime lock is changed.

The source diff contains37 production lines and70 browser-regression-test lines across3files. Frozen dependencies, changed-file type/lint/format checks and production ESM generation passed. The complete unchanged SsbCommandLineHandler test module and repository harness ran in the real candidate browser: all4tests pass, including a real ordinary-window/PWA pre-layout category test. Source byte hashes match the subsequently committed immutable head (candidate-source-commit-match.json).

## Product origins: keep these distinct

1. **Baseline full package:** our previous unsigned stable-v2-validation run42068055038, installer13436721142, source8249e234, ZIP SHA256fc338fa4cd3307c7a8937b2b99c97c18a1665e23b3b561680662fddfc3b336e5. Original complete intake is ../full-build/3032/product-intake.json.
2. **Quick candidate:** a separate copy of that complete package with6source-generated ESM resource changes; all native and other product files match. It is not a new complete build. candidate-resource-provenance.json preserves the precommit source snapshot and exact resource hashes; candidate-source-commit-match.json maps those unchanged source bytes to2deb165d.
3. **New full package:** exact-head unsigned package.yml run42077174018, using the unchanged compatible authenticated Runtime8965ffc346d426ccdf919bd18585e4d4a0c21654 / native159.0.1 / BuildID20261103153232. CI completed successfully. The final installer artifact13439144608 has expected ZIP SHA256febce14e382da315a7579464f1052768a0e51854a752de8a926ee898db6ce996; exact-byte authentication and final native tests are recorded under full-package. No native Gecko recompile or signed-distribution qualification is implied.
4. **Standard browser CI:** run42077116984 completed all5jobs successfully,281browser modules pass/0fail. Authenticated artifact13438134467 ZIP SHA2561856010f6fc5081dc86292530be029fe05e612e15df5a2034f0e7af183b6d054. The new case is included in the successful SsbCommandLineHandler module. This is separate from the package build and local exact-module execution.

Cloud B's strict package/XPT work was mentioned by the user; its artifact was not supplied or inspected here. None of the above products is silently identified as Cloud B's output. No helper XPT was added in our source, quick candidate or package route.

## Native comparisons completed before full-package intake

Debian13 / KWin6.3.6, dedicated normal session bus, nested native Wayland. No identity mock, compositor rule or forced taskbar rollout is used. The runner reads actual xdg_toplevel.set_app_id protocol and KWin resourceClass/desktopFileName. A title-only QA marker makes ordinary browser windows unambiguous; it does not assign windowclass/app_id. Synthetic UI creates each fixture. The original old profile/home are preserved. The isolated migration copy retains SSB IDs and desktop-entry bytes; only its automation port is adjusted, and executable/profile command arguments are relocated at execution time.

| Condition | Baseline3032 full package | Source-generated candidate copy |
|---|---|---|
| Ordinary browser then PWA | Correct | Correct |
| PWA then ordinary browser | Browser inherits Alpha | Browser canonical; PWA distinct |
| Alpha, Beta, ordinary browser | Browser inherits Alpha | Browser canonical; both PWA distinct |
| Existing profile / old launchers | Browser inherits Alpha | Browser canonical; original IDs retained |
| Browser-first custom --name; remote PWA; another normal window | Custom identity retained | Custom identity retained; PWA distinct |
| PWA first, then real private browser window | Private browser inherits Alpha | Private browser canonical; actual private flag remains true |

All6candidate-copy cases pass native strict checks. A supplementary X11 cold Alpha→normal→Beta smoke checks actual managed WM_CLASS and passes; its fixture HTTP server was offline, so it is strictly an identity assertion. Each completed test normally quits, preserves sandbox6/rolloutfalse and full product inventories, and leaves no owned browser processes. Original migration profile/home inventories also remain unchanged. Full raw and compact results distinguish valid product observations from tooling failures.

## Tooling failures retained separately

The initial nested Wayland socket failed before product launch because AF_UNIX path+NUL exceeded108bytes. The runner now uses a dedicated0700 short /tmp runtime path. The first custom-name control opened a new tab rather than the required second normal window; it was corrected to the real --new-window CLI. The first archive-copy helper reused mutable ZipInfo offsets; it was corrected on a new disposable copy, with original product hashes unchanged. None is a product regression or a failed condition converted into a pass.

## Remaining qualification

Original Manjaro/KDE and actual Outlook/Teams/taskbar icon pixels remain distinct from these native-identity tests. The existing all-invalid legacy-record random fallback limitation is outside this change. Browser-first custom --name is verified; a custom --name combined with a PWA-first start still gives later ordinary windows the canonical browser identity by design. No merge, release, distribution change, Runtime-lock change or Done transition is authorized/performed.

## Final unmodified complete-package native acceptance

The authenticated unsigned artifact13439144608 from exact source2deb165d / run42077174018 was safely re-extracted without resource substitutions. The final untraced suite is **full-package/actual-qa-r2/suite-result.json**, qualified=true,6/6cases: ordinary browser first, PWA first, multiple PWA, prior QA profile/old launchers, browser-first custom name plus second ordinary window, and real private window. Native Wayland protocol and KWin identity checks agree. Every group preserves complete product/base inventories and original migration profile/home/store, sandbox6 and taskbar rolloutfalse; all owned browser processes exit normally. No validation allowlist, identity mock, permission restriction or protection change was introduced.

### Initial full-package inventory anomaly retained

The first full-product run (full-package/actual-qa) measured all4core native identity cases correctly, but its strict invariant stopped: the first ordinary UI process created a new empty .parentlock and a lock symlink in the writable unpacked application directory. No preexisting delivered file bytes changed. Custom/private controls were stopped before launch, so that suite remains qualified=false. Its mutated product tree, logs and product-mutation-observation.json are preserved. An identical-archive main-process strace diagnostic did not reproduce the app-directory lock operations; because tracing can affect timing it was not used as final acceptance. The subsequent fresh **untraced**6case suite passed the unchanged strict gate with no new lock files.

The lock's creation source remains unknown; it is not claimed fixed or silently classified as infrastructure. Delivered native files27/27, component manifests and profile-manager actor bytes match the previous3032 package; both tar archives have no root locks. See profile-lock-audit/report.md and full-package lock diagnostic records. This anomaly is distinct from the reproducible Wayland grouping defect fixed by3040. No lock-source patch was guessed.
