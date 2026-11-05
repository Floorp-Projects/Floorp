# PR3030 standard frontend production preprocessing

Result: **standard before-mach phase completed successfully**, not a completed full Linux product/package.

GitHub PR3030 was checked immediately before and after this work. Both observations identify open Draft head `e55152c559a141001a375ac77098d141b0db3ca7`, base `984509d2053be7bae9027133bda7259cd706b208`. An independent detached worktree was created at `/workspace/qa-linux-full-build/floorp3030`; its AGENTS.md was read. Source tree SHA is `80f2dba56e37ad7fac221c9ac258b9ea4c5abfd3`. The tracked checkout is clean after preparation/build. No PR/source/lock modification was made.

Locked Runtime is `8965ffc346d426ccdf919bd18585e4d4a0c21654`, as specified by the unchanged floorp-runtime.lock.json (SHA256 `5838f685f4b375ca69dfaf10f1054f6b7fe1e90290cfb97c9f53514022937fe5`). Deno lock SHA256 is `1eb6c7fa6538a850e216736323edb3eedee85754cb0500e1f171ab945af62d4e`.

The work used dedicated HOME `/workspace/qa-linux-full-build/home3030`, Deno2.9.6 and the existing Deno dependency cache. It did not source env.sh; MOZ_DISABLE_CONTENT_SANDBOX was absent from the child environment. Inherited network/proxy/CA configuration was preserved without printing credential values.

Commands ran sequentially from the independent worktree:

1. `deno install --frozen` — exit0, 1.68 seconds.
2. `NODE_ENV=production deno task feles-build build --phase before-mach` — exit0, 92.49 seconds, standard builder reports `Build complete`.

This used the normal Symlinker and all production component tasks. All11 directories required by the source moz.build are present, with each component's normal moz.build and jar.mn: content, skin, startup, resource, settings, modal-child, newtab, welcome, notes, profile-manager and aboutDialog. The normal output uses component directory symlinks into **this same worktree**; the inventory follows those links and records their resolved targets. It contains1143 staged frontend/static files totaling43,315,341 bytes. The canonical path/size/hash list digest is `b2f859b00dc7aa07b019bdff76c118031ee6d213d0ec79b18f87ba55a585b892`. An initial inventory implementation did not traverse directory symlinks; the saved inventory script and final JSON were corrected to follow and validate them.

The actual generated content jar.mn includes exactly the intended brand.properties override. Its SHA256 is `c8604b32d5487b91db8f3192d09052e842b36dbf9676921ce3b8191affa1982d`. This manifest check is a supporting output check, **not** a substitute for the full standard preprocessing command or the remaining native/package stages. No installed product/omni archive was manually modified in this work.

Root can connect the locked Runtime's `noraneko` source link to `/workspace/qa-linux-full-build/floorp3030` for the remaining normal configure, mach build, after-mach/integration and packaging stages. Those stages and actual full-package locale execution are not completed by this report and remain root's responsibility.

Evidence: pr-info.json/pr-info-after.json, run_frontend_build.py, build-record.json, frozen-dependencies.log, before-mach.log, run.log, frontend-summary.json, frontend-files.json, generated-content-jar.mn and inventory_frontend.py. No existing QA browser/Flatpak profile was re-exercised.
