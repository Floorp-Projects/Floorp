# Draft3048 final standard CI evidence

Run42112961086 at exact head0e80f14dc439dde7817c0620f0fc112d54dfba80 completed **success** on attempt1, all5jobs successful. Ubuntu standard browser suite: **284modules passed,0failed**. Downloaded Firefox8files/16tasks all passed. No job retry or local dev-mode boot was performed by the monitor.

## Authenticated actual execution

Ubuntu artifact13456440170 (`test-logs-ubuntu-24.04`) was retrieved through the connected GitHub artifact tool. ZIP size108455 and SHA256 **a67d1c09ef721bfab4db0e0ac66f67f4aaa1de7be4093ba38f9635a48f402568** match its API metadata, bound to run42112961086/head0e80f14d. No signed URL or credentials are saved.

Both new modules successfully executed their `runAllTests`: lepton-container-geometry.test.ts and fluerial-container.test.ts. Each exact-head module declares2cases; those counts are source interpretation, not separately logged individual-case success lines. Existing lepton-compat.test.ts, Photon container-indicator.test.ts and fluerial-groups.test.ts controls also completed successfully. See target-test-execution.json and artifact-verification.json for exact line-number excerpts.

## Preserved earlier failure and strict correction

Prior run42110920041/head3a9f99ae failed283pass/1fail. Both new modules already passed, but the existing strict bundled-CSS assertion omitted the appended LEPTON_CONTAINER_GEOMETRY_CSS. All original failure metadata, digest-authenticated artifact/logs and classification are preserved under **old-head-3a9f99ae/**, not overwritten or called infrastructure failure.

The new head changes only the existing composition test import/expected CSS concatenation. source-subject.json and strict-composition-head-delta.patch prove the exact one-file delta and byte identity of both production CSS files and both new modules between heads. The strict assertion was extended to include the actual new chunk; it was not weakened. New CI success is a new-head validation, not an old-head retry.

## Qualification boundary

This is repository **standard development-harness CI** against the unchanged locked Runtime source8965ffc346d426ccdf919bd18585e4d4a0c21654. It is separate from sandbox6 complete production package verification, original Windows12.20.2, physical GPU/highDPI and signed release qualification. Parent owns those full-product reports. This monitor made no source/PR/Jira edits, merged nothing, and dispatched no local normaltest-mode/protection changes.
