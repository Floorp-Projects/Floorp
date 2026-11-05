# Full production package verification

## Subjects and build scope

Requested PR3030 source e55152c559a141001a375ac77098d141b0db3ca7 and PR3032 source 8249e234adfbb3e877e4a20cf5f57d7264d9a447 use compatible Runtime 8965ffc346d426ccdf919bd18585e4d4a0c21654 / Gecko159.0.1 / BuildID20261103153232, unchanged lock. The comparison baseline is 984509d2053be7bae9027133bda7259cd706b208; latest main has since advanced, recorded separately in main-ref.txt.

The existing package.yml pipeline rebuilds all Floorp application sources in production, runs mach artifact-mode configure/build, standard injection and packaging, using authenticated compatible native engine artifacts. This qualifies as a complete production application package pipeline, **not a full recompilation of Gecko C++/Rust**. Earlier manifest-only and two-module product-copy experiments remain separate evidence.

## Execution

- PR3030 unsigned package validation: https://github.com/Floorp-Projects/Floorp/actions/runs/42067997767
- PR3032 unsigned package validation: https://github.com/Floorp-Projects/Floorp/actions/runs/42068055038
- Both standard preflights required the configured Runtime-read binding and validated exact producer/manifest/native artifact contracts successfully. No secret values were read.
- Supported inputs stable-v2-validation / skip_signing=true / signed_validation=false / beta=false select existing package-only behavior. No release publisher, signing, distribution update, Runtime lock change, merge or Done transition was requested.
- The audit identified the existing OpenBot completion metadata notification, not a source-defined publication chain.
- Local isolated locked Runtime checkout completed. Ordinary bootstrap required administrator authentication; the documented --no-system-changes bootstrap then exited0.
- PR3030 complete standard frontend before-mach generation exited0, all11components /1143files. This preparatory result alone is not a complete packaged product.

All three complete package workflows finished successfully without retries. Exact final artifacts:

| Subject | Run | Installer artifact | ZIP SHA256 |
|---|---|---|---|
| Baseline984509d2 |42068377281|13435702404|83667970ddcfc2d3f350367f511106ddaf6f52c5ca8ae3069a44870aee22913c|
| Draft3030 e55152c5 |42067997767|13435129152|96ebf4afca0028715039d5154a27fd030e8150cb8117b0249d91eb576411d3fc|
| Draft3032 8249e234 |42068055038|13436721142|fc338fa4cd3307c7a8937b2b99c97c18a1665e23b3b561680662fddfc3b336e5|

Each downloaded ZIP was authenticated against its exact API size/digest/run/source and safely extracted. The products are unmodified complete workflow packages, with complete inventories before and after the runtime probes. Candidate3032 X11 and native nested Wayland PWA identity tests have completed, with both PWA windows matching their own generated launchers; baseline and locale comparisons are recorded separately as they finish.

## Identity metadata distinction

The packaged root application.ini explicitly states it is not used. It and platform.ini contain regenerated packaging-time metadata, while the running unmodified3032 product reports Services.appinfo.version159.0.1 / appBuildID20261103153232, matching the pinned authenticated native Runtime. Keep both observations. The unused file alone does not demonstrate a native Runtime mismatch. This task does not claim signed distribution or final multi-platform release qualification.

## Wayland condition retained

Cold Alpha → Beta → ordinary browser on candidate3032 gives each PWA its correct distinct native app_id/KWin identity, but the ordinary browser inherits Alpha's process identity. This is recorded separately rather than claiming all cold-start grouping behavior is fixed. Baseline comparison determines whether it is pre-existing.


## Completed full-package comparisons

- **3030:** four fresh-profile cases completed. Baseline ja/es-CL reproduces Firefox branding, exact Firefox64px icon and regression failure. Candidate ja/es-CL gives Floorp branding, exact Floorp64px icon and regression success. Both signed language packs remain active (signedState2), actual locale matches request, localized visible menus remain correct, sandbox6, no disable variable. Each install/main browser exits normally; all package files are unchanged. The generated branding override is read directly from the final root omni.ja chrome manifest, not manually inserted. The whole regression module bundle executes in the actual full product; this is distinct from the standard top-level CI runner.
- **3032:** four full-package runs (baseline/candidate × X11/native nested Wayland), each freshly installing2PWA through that same product UI and cold-launching its own generated Exec. Baseline X11 gives both PWA classfloorp; baseline Wayland makes Beta inherit Alpha nativeapp_id. Candidate gives both PWA their respective launcher identities on both protocols. Native Wayland protocol and KWin rows corroborate this. All36 legacy API windows open across4runs, with baseline helper errors separately recorded. Normal Quit, sandbox6, rolloutfalse, no owned processes left and complete product inventories unchanged.
- **Residual:** ordinary browser opened after a PWA inherits Alpha native Wayland app_id in both baseline and candidate. It is pre-existing and outside the PWA-window identity fix; no claim of complete browser grouping remediation. The original Manjaro/CachyOS desktop, real Outlook/Teams, unusable-all-fields legacy records, other locale/theme/channel and signed-distribution qualification remain.

No PR code change was required in this phase, and no speculative fix, Runtime lock change, signing, publication, merge or Done transition was performed.
