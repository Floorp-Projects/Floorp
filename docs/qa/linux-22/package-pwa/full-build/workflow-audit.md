# Draft3030 / Draft3032 production-build workflow audit

2026-11-06 JST. Read-only audit completed; no workflow was dispatched, no build or signing was started, no secret value was read, and no source, Runtime lock, delivery or publication configuration was changed. Evidence and machine-readable input requirements are in `workflow-audit-summary.json`.

## Conclusion

There is an existing **Linux-only package workflow** that builds the exact Floorp frontend source in production mode, combines it with an authenticated prebuilt native Runtime using `mach` artifact mode, injects the remaining tracked assets and creates the full package. The applicable unsigned package-only route is **`package.yml`, `provenance_mode=stable-v2-validation`, `platform=Linux-x64`, `skip_signing=true`, `signed_validation=false`, `beta=false`**, pinned to the exact PR source and locked compatible Runtime. It uploads workflow artifacts and does not itself call the public-release publisher.

The higher-level `package_and_publish_release.yml` supports **`validation_only=true`**, which also prevents signing/publication but builds all four platform packages, executes five native package verifiers and assembles a validation bundle. That is broader than the requested Linux-only build. The literal input names **`existingpackage_only` and `build_only` do not exist in the audited definitions**; do not dispatch with invented inputs or assume those names activate a safe mode.

`firefox_runtime_candidate.yml` is a **locked-material tracking-drift audit**, not a Firefox/Runtime or Floorp product build. Its name is misleading if treated as a compiler producer: it checks out the tracking ref as data, collects/triages a bounded test projection and emits a drift report.

These package routes rebuild the **Floorp application source plus a precompiled native engine**. They are not a fresh compile of all Gecko C++/Rust. Both PRs change frontend/ESM/branding source and tests, not native engine source. A full native Gecko source build would be a separate locked Runtime task with a larger environment and separate native evidence; do not rename artifact-mode packaging as that result.

## Immutable audited subjects

| Draft | Exact requested head | Changes from base984509d2 |
|---|---|---|
|3030|`e55152c559a141001a375ac77098d141b0db3ca7`|loader-features Vite config and legacy-branding test|
|3032|`8249e234adfbb3e877e4a20cf5f57d7264d9a447`|PWA command-line handler, Linux support and command-line tests|

The two heads have identical Git blobs for `package.yml`, `package_and_publish_release.yml`, `firefox_runtime_candidate.yml`, `validate-runtime-provenance.yml`, `floorp-runtime.lock.json` and `tools/feles-build.ts`. Blob IDs are recorded in the JSON summary. This avoids relying on the later shared worktree's moving HEAD for workflow interpretation.

Both locks select:

- Runtime repository `Floorp-Projects/Floorp-Runtime`, commit `8965ffc346d426ccdf919bd18585e4d4a0c21654`, tree `53216e352ae9a27c402b707e1384bb5d62e9db7e`, release/tag `daily-1179`, release ID433288890 (`immutable:false`, so validate current identity rather than assuming GitHub marks the release immutable).
- Linux x86_64 Runtime release asset ID707330202, `floorp-linux-x86_64-moz-artifact.tar.xz`, size86774264, SHA256`cb9b01283d5ce3167f4ba046ee7b39ae9d98a692e1beba73bd9f85f460157772`.
- application.ini companion release asset ID707330199, SHA256`742bd72ce869d127abefdcbfdd0a0e785786dbd7afd133c6a930833e54d6b956`.
- Engine159.0.1 / BuildID20261103153232.

**Release asset IDs are not GitHub Actions artifact IDs.** The v2 CI route additionally requires a currently available successful canonical Runtime workflow run and its exact manifest/target Actions artifacts. A subsequent read-only live lookup during this audit confirmed producer41664957250 and the IDs/digests below; secret-binding availability was not inspected. The compact snapshot is `package-dispatch-readiness.json`, without account/email metadata.

## Linux-only CI route and required inputs

Use the existing workflow definition at each exact PR head. The dispatch ref/branch must still resolve to that head when used: `floorp_head_sha` is explicitly required to equal the caller's `github.sha` ([package.yml L485](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L485)). A later branch push needs a fresh audit/subject record; passing an old SHA while running a new branch is rejected.

| Input | Required value / origin |
|---|---|
|platform|`Linux-x64`|
|beta|false|
|provenance_mode|`stable-v2-validation`|
|skip_signing|true|
|signed_validation|false; do not choose the optional signed validation path|
|runtime_repository|`Floorp-Projects/Floorp-Runtime`|
|runtime_head_sha|the locked8965ffc… commit above|
|expected_build_id|20261103153232, and must agree with selected producer run creation-time identity|
|floorp_head_sha|3030's exacte55152c… or3032's exact8249e234… SHA|
|runtime_workflow_run_id|currently verified completed/success canonical `.github/workflows/daily-build.yml` run at that Runtime SHA|
|runtime_manifest_artifact_id / digest|same producer's nonexpired `floorp-runtime-build-manifest-v2` artifact ID and API SHA256 digest|
|runtime_artifact_name|`floorp-linux-x86_64-moz-artifact`|
|runtime_artifact_id / digest|same producer's nonexpired matching Linux target Actions artifact ID/digest|
|runtime_artifact_workflow_run_id|empty; legacy and v2 fields cannot coexist|

A configured cross-repository Actions-read binding (`runtime_artifact_token` or `PAT`) is required by the workflow; only the names/requirement were read, not values. See [L240](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L240).

The workflow checks supported unsigned-validation flags, canonical Runtime repository, SHA/BuildID syntax, exact caller SHA, correct canonical producer workflow, completed/success state, producer SHA/repository, creation-time BuildID, artifact IDs/names/digests/expiry and producer ownership. It downloads by immutable artifact ID, checks the exact extracted top-level layout, application.ini BuildID and compiled-source provenance when present. Evidence: [L461](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L461), [L539](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L539), [L588](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L588), [L703](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L703), [L714](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L714).

The package workflow is contents/actions-read and its steps named “Publish dist/MAR” are **Actions artifact uploads**, not a GitHub Release/update-server publication ([L211](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L211), [L2417](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L2417)). Windows/Mac signing paths are separately conditioned; do not use signed_validation or invoke a publisher for this task.

## Higher-level unsigned validation route

`package_and_publish_release.yml` accepts producer run ID, manifest artifact ID and `validation_only`. Its supported validation flag is declared as “Build, verify, and assemble without signing or publishing” ([L16](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package_and_publish_release.yml#L16)). It feeds `stable-v2-validation`, exact caller SHA and skip-signing into the package jobs ([L153](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package_and_publish_release.yml#L153)); assembles only after all package/native jobs ([L424](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package_and_publish_release.yml#L424)); and gates the publisher behind `validation_only != true` ([L518](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package_and_publish_release.yml#L518)). Its default is **false**, so omit-no-flag use is not appropriate.

Validation mode allows non-default Runtime branches and skips the production-only equality-to-lock check in `validate-runtime-provenance.yml`. Therefore **manually require the selected Runtime commit to equal the unchanged lock for this task before using it**; the flag alone does not guarantee lock compatibility ([validator L183](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/validate-runtime-provenance.yml#L183)). It still validates manifest/target digests and ownership. No request to relax that contract is proposed.

`verify-release-artifact.yml` is reusable, not a standalone dispatch. A direct Linux package run by itself does not automatically execute that wrapper's native verifier jobs. Final-product integration checks can be run locally on the downloaded exact package, or the existing higher-level validation route can produce the strict package-provenance records. Package build success alone is not a native GUI regression result.

## Local complete application build path

There is no single local “full package” feles command. The existing production pipeline is two feles phases **around** a locked Runtime artifact-mode `mach` build:

1. Use a new isolated Runtime source root at the locked8965ffc… commit, with the exact3030 or3032 Floorp checkout in its `noraneko/` child. Do not build in or modify the shared sibling Runtime checkout. Apply the existing upstream/common source patches as the workflow does, with their normal checks; the common patch loop accepts only already-applied or cleanly applicable patches ([package L639](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L639)).
2. Install Deno dependencies/scripts, normal artifact-mode bootstrap/toolchain and platform mozconfig; copy the tracked branding and write the version inputs. The existing workflow uses `mach bootstrap --application-choice browser_artifact_mode` and enables artifact builds into `obj-artifact-build-output` ([L869](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L869), [L955](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L955)).
3. Build all production Floorp frontend assets: `NODE_ENV=production deno task feles-build build --phase before-mach`. This builds production assets but is not a complete product by itself ([L1344](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1344), [feles L237](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/tools/feles-build.ts#L237)).
4. Use the authenticated matching Linux native Runtime archive: `MOZ_ARTIFACT_FILE=<exact verified archive> ./mach build` ([L1397](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1397)). For a release-asset local intake, the existing Runtime-lock CLI supports `validate-lock`, `validate-release-metadata`, `validate-native --out` and transactional `install-native`; it verifies release identity/hashes/INI. `install-native` only installs into `_dist/bin` and is not a replacement for the complete mach/production pipeline ([CLI L31](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/tools/runtime-lock/runtime_lock_cli.ts#L31)).
5. Run `deno task feles-build build --phase after-mach`, then the remaining tracked omnijar patches/buildid/prefs operations and `mach package` as in the workflow ([L1441](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1441), [L1523](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1523), [L1602](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1602)). `after-mach` targets a **parent Runtime root's** `obj-artifact-build-output/dist/bin`; the checkout layout matters ([injector L43](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/tools/src/injector.ts#L43)).
6. Preserve exact source SHA, Runtime/source/artifact hashes and new product package hashes, then launch the resulting product with a fresh isolated profile and normal protections to run the per-PR regression. Do not substitute only a built omni.ja into the old release and describe that as the full build.

`feles-build stage` builds production assets but runs a **development-layout** browser; it is useful integration evidence but does not establish final-package qualification ([feles L118](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/tools/feles-build.ts#L118)). The command reference confirms only `build --phase before-mach|after-mach` ([docs L333](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/docs/development/reference/command-reference.mdx#L333)).

Existing release config disables compiling tests and the package script contains an existing auxiliary-empty-archive policy adjustment in `packager.mk` ([L966](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L966), [L1638](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L1638)). These are recorded pipeline behaviors, **not new changes proposed by this audit**. Do not infer source-test execution or validation of absent optional archives from package success, and do not add further validation skips to make this task pass.

## Runtime producer / tracking workflow boundaries

The exact locked Runtime's [daily-build.yml L26](https://github.com/Floorp-Projects/Floorp-Runtime/blob/8965ffc346d426ccdf919bd18585e4d4a0c21654/.github/workflows/daily-build.yml#L26) declares its own `validation_only` for desktop artifact build/verify without publishing; FTP/release publication gates exclude that flag ([L412](https://github.com/Floorp-Projects/Floorp-Runtime/blob/8965ffc346d426ccdf919bd18585e4d4a0c21654/.github/workflows/daily-build.yml#L412), [L483](https://github.com/Floorp-Projects/Floorp-Runtime/blob/8965ffc346d426ccdf919bd18585e4d4a0c21654/.github/workflows/daily-build.yml#L483)). This was fetched read-only at the immutable source ref because the saved sibling checkout has a different HEAD. Starting that producer is outside this audit and is unnecessary if the matching existing locked engine artifacts are available.

The Floorp `firefox_runtime_candidate.yml` explicitly treats the tracking checkout as data, uses `firefox-tests:collect --candidate` and static triage ([L40](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/firefox_runtime_candidate.yml#L40), [L70](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/firefox_runtime_candidate.yml#L70)). Running it would not resolve either PR's missing complete production product build.

## Evidence still required before execution

- Confirm each source branch still identifies the requested exact SHA and no overlapping build writes into another task's checkout/profile.
- Revalidate the selected live successful producer and artifacts below immediately before dispatch; their availability was established at the audit lookup, not guaranteed indefinitely.
- Confirm configured cross-repository Actions-read capability through its normal binding/CLI, without printing token values; no signing credential is needed for the unsigned path.
- Choose Linux-only package-plus-product-test or the broader existing all-platform validation route; do not use production defaults or a publisher. No Runtime-lock changes, signing, release/tag, distribution update or merge are part of either proposed task scope.

## Live producer readiness and exact dispatch bodies

Read-only API lookup confirmed [Runtime run41664957250](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41664957250), workflow `.github/workflows/daily-build.yml`, completed/success, attempt1, canonical head repository, headSHA8965ffc… and branch`nora-0.2.0`. Creation`2026-11-03T15:32:32Z` agrees exactly with expected BuildID20261103153232.

| Actions artifact | ID | API digest | Expired / expiry |
|---|---|---|---|
|floorp-runtime-build-manifest-v2|13274392012|`sha256:2b6a1b20190dc7016957ca25f72f8e83f267de51458b515153fd209bb90d9138`|false /2027-02-01T15:32:34Z|
|floorp-linux-x86_64-moz-artifact|13272530069|`sha256:ed5a4f97ee7bf42c817f3146d5a843e095710dcc3be25fae0af0bf47274b739a`|false /2027-02-01T15:32:34Z|

Both report the same producer run/head and positive sizes. The manifest ZIP was subsequently downloaded read-only through the connector, matched its API SHA256 exactly, and contained only `runtime-build-manifest-v2.json`. Its head/run/attempt/BuildID and Linux target ID/digest match the prepared payloads (`runtime1179-build-manifest-v2.json`). This audit did not download the Linux Runtime target archive itself; the protected package workflow's standard extraction/layout/source checks remain required.

Live PR metadata also confirmed both Drafts remain open/unmerged and are **same-repository heads** in `Floorp-Projects/Floorp`:

- 3030: branch`codex/fix-linux-langpack-branding`, SHA`e55152c559a141001a375ac77098d141b0db3ca7`.
- 3032: branch`codex/linux-pwa-window-class`, SHA`8249e234adfbb3e877e4a20cf5f57d7264d9a447`.

`package-3030-dispatch-payload.json` and `package-3032-dispatch-payload.json` contain complete REST request bodies for **`POST /repos/Floorp-Projects/Floorp/actions/workflows/package.yml/dispatches`**. They preserve all v2 identity fields and supported unsigned flags, with the exact branch/head bindings above. These files are preparation only; **this auditor did not send the POST**. Cross-repository token readiness is intentionally left to the existing workflow preflight, without reading its secret value.

### Fork, owner and branch constraints

`stable-v2-validation` deliberately does not apply the production-only canonical Floorp-owner/default-branch restriction at package.yml L386. It still requires a syntactically valid Floorp repository and exact `floorp_head_sha == github.sha`. Thus the validation mode can technically run on a fork if that fork has its own enabled workflow and configured Runtime-read capability; this is not proof that an arbitrary fork has secrets or permissions. This task's actual heads are canonical-repository branches as confirmed above, and the prepared API endpoint is canonical, not a fork.

The Runtime repository is always fixed to `Floorp-Projects/Floorp-Runtime` for v2. Canonical Runtime head-repository and SHA checks still apply, while the production-only Runtime default-branch/event restriction is guarded by `productionV2` ([L558](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/package.yml#L558)). The selected producer happens to be the canonical scheduled/dispatch-compatible production branch and exact unchanged locked SHA; no need to relax any check or move the lock.

### Completion/publication chain audit

All **21 workflow YAML files at the immutable3030 head** were scanned and their blob IDs recorded in `workflow-trigger-scan.json`; the3032 change list contains no workflow changes. There is one `workflow_run` event listener: `openbot-events.yml` watches completed workflows with wildcard`["*"]` ([L12](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/openbot-events.yml#L12)). Therefore a canonical package completion **can trigger that metadata notification**; it would be incorrect to claim no downstream workflow at all.

That listener has `permissions:{}`, does not check out source or download artifacts, and forwards bounded run metadata to its configured OpenBot webhook ([L16](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/openbot-events.yml#L16), [L134](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/.github/workflows/openbot-events.yml#L134)). It does not call a publisher, update distributor, release API or merge API in this source. Its documented workflow completion is not an approval or bypass of required checks ([OpenBot docs L55](https://github.com/Floorp-Projects/Floorp/blob/e55152c559a141001a375ac77098d141b0db3ca7/docs/development/reference/openbot-automation.mdx#L55)). The external webhook service's implementation is outside the inspected repository source and was not audited here; do not claim a universal guarantee about external organization automation.

There is **no source-defined publication/distribution job chain from standalone package completion**. `publish_release.yml` and`publish_alpha.yml` are reusable/manual publisher entrypoints; `package_and_publish_release.yml` and`package_and_publish_alpha.yml` are separate manually dispatched parents. Calling their child`package.yml` alone does not call the parent or its later publisher. No other workflow-run listener or repository_dispatch event listener was found at the audited Floorp ref. Package.yml's Linux path has no release/upload-server/Updates/PPA dispatch action; its “Publish” steps upload Actions artifacts. This establishes the requested source-level boundary, with the explicit OpenBot notification limitation above.
