# KAN-26 combined-source regression qualification

Exact Draft3048 source3a9f99ae12dbb221f63b28ec210b04fafcf2be15 combines the non-Photon fixes onto canonical3041. Its actual two new test modules and vendored CSS were bundled from that clean source worktree and executed sequentially in an actual immutable native159.0.1 / BuildID20261103153232 product. build-provenance.json pins eight imported source/blob/SHA256 subjects plus the exact resulting bundle digest.

| Module | Declared cases | Result | Time |
|---|---:|---|---:|
| lepton-container-geometry |2 |runAllTests passed, done2/2 |12337ms |
| fluerial-container.test.ts |2 |runAllTests passed, done2/2 |1428ms |

The first module uses actual vendored Lepton CSS and the combined compatibility constants in a controlled closed-shadow layout fixture, including native-token-only inputs and non-Photon scope gates. The second creates genuine native container tabs/identities and applies the candidate Fluerial stylesheet at its production stylesheet origin to test native stacking and orientation. The repository test harness collects failures and throws if any declared case fails. The four declared cases completed through both runAllTests calls; these are not four independent full-product UI matrix executions.

Fresh dedicated profile, owned Xvfb114/normalDBus, content sandbox6 and no disable variable. Native product resource hashes were unchanged, normal in-app quit exited0/forcedfalse, owned processes0, X114 socket/lock removed. No Photon36 or whole80 product UI comparison was repeated. No source file, package, experiment, certificate/CSP setting or shared checkout was modified by this subtask.

This is candidate **combined-source regression-fixture qualification**, not the completed candidate production package or standard top-level browser runner. The underlying authenticated complete product is the earlier main29a3 baseline package; test code/styles are deliberately mounted by the tests and do not turn its resources into a candidate product. Parent owns the separate package/CI qualification.

Replay: node build.cjs, then dbus-run-session -- python3 run.py --product <authenticated native159 product>, using a fresh evidence directory or fresh copy of the script/bundle so no profile is reused accidentally. The fixed native path in this retained run points to the authenticated main product documented by the parent. Validate retained evidence with python3 validate.py.

Exclude home-direct/ and runtime-direct/ from publication. Keep build/test scripts, compact JSON, the source-pinned bundle and logs only.
