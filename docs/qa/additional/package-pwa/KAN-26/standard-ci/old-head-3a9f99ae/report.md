# Draft3048 standard CI: recorded failure at exact head

Run42110920041 at head3a9f99ae12dbb221f63b28ec210b04fafcf2be15 completed **failure** on attempt1. Smoke/Windows/macOS/ARM succeeded. Ubuntu executed the full browser suite: **283modules passed,1failed**. Downloaded Firefox8files/16tasks passed. No job was retried.

The sole failing module is the existing `lepton-compat.test.ts` bundle-composition assertion. Its expected CSS concatenation includes color+compat+Photon chunks but does not yet include the newly appended `LEPTON_CONTAINER_GEOMETRY_CSS`. This is a real test-contract failure requiring the parent to update the explicit expected concatenation; it is not an infrastructure failure and is not hidden or reclassified as a pass. No monitor source/PR/Jira changes.

Both newly submitted modules completed successfully:

| Module | Standard runAllTests | Declared cases |
|---|---:|---:|
| fluerial-container.test.ts |success750ms |2 |
| lepton-container-geometry.test.ts |success12098ms |2 |

Existing `container-indicator.test.ts` and `fluerial-groups.test.ts` controls also succeeded. Declared case counts are exact-head source interpretation, not independently logged per-case success lines.

Ubuntu artifact13454592586 (`test-logs-ubuntu-24.04`) was retrieved through the connected GitHub artifact tool. ZIP SHA25664491b816ef32cc8afa0f7e1cc579d4b87f2c20c460e6645b3c33b08468ff752 matches the API digest and119139byte size, bound to the same run/head. No signed URL is saved. See artifact-api-metadata.json, artifact-verification.json, failure-classification.json and evidence/_dist__ci-combined-browser-test.log (targetlines364–367,739–740; failure369–370; summary1290–1291).

This is the repository standard development harness, distinct from sandbox6 complete production product qualification, original Windows12.20.2, physical GPU/highDPI or signed distribution. Parent's fullpackage run remains separate.
