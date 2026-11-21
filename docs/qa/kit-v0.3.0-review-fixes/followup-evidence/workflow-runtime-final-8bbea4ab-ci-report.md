# Independent final-head public CI observation: Runtime #84

The final pushed Runtime #84 head `c9027879e053c9031c7f3e27495f6d415d36c10f` passes its ordinary hosted QA3 shadow CI. Independent official GitHub connector reads bind [run 37954821322](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/37954821322), attempt 1, event pull_request, `.github/workflows/qa3-shadow.yml`, PR #84 and base `91e94b73617730d54ffee3d4ea42ff3a91463c7d`. The run and its single expected `contracts` job `113902544655` are completed/success on ubuntu-24.04 / GitHub Actions.

All required checkout, contract, policy preflight and artifact preservation steps are independently confirmed completed/success. Decoded raw logs from the official job-log connector contain:

```text
Ran 134 tests in 49.212s
OK
```

This **134** is the workflow's `test_qa3_*.py` subset. Root's separately executed whole local suite is **203**; the predecessor whole local suite was **190**. This reviewer did not execute the whole local suite or infer test counts from a successful job. The initial observation helper's assumed count 203 was incorrect; its initial record is retained, and the corrected exact raw-count record is `workflow-runtime-final-8bbea4ab-ci-final-summary.json`. There was no GitHub test failure.

The checkout log explicitly merged c902 into final #83's 91e94 and records checkout merge SHA `7e4b7ce818aadc5cfd41f58a79b82ed3487d4b6f`; QA3_RUNTIME_HEAD is that same merge SHA. PR head and checkout merge SHA have distinct purposes and are both retained. The raw log SHA-256 is `9bff23cd43eb073f2b67a711c71d724ac7ba5ce093c5c7655afca3e07ae4de45`. Artifact upload reports `qa3-runtime-37954821322-1`, ID `11627881327`; artifact bytes were not downloaded in this observation.

Job-level continue-on-error remains present in the fixed workflow. PASS here is supported by the required contract step's actual success plus the raw unittest result, alongside exact run/head/attempt binding. A canceled/skipped required step or a previous-head run would not satisfy this check. Policy preflight is read-only evidence; native/product qualification remains UNVERIFIED and publicationAuthorized false.

The separate portability review `runtime-independent-review-02.md` reports 47 actual native-I/O test methods successful, original parent-init regressions and supplemental owned-process cleanup cases green before independent recovery, at exact c902 / 8bbe / 0dae. This is attributed peer evidence, separate from this reviewer's 71 synthetic/control-policy checks and public CI reads. The operator/worker-loss/private external job boundary remains UNVERIFIED.

The previously frozen independent protocol/source/workflow report is `workflow-runtime-final-8bbea4ab-report.md`, with `workflow-runtime-final-8bbea4ab-review-frozen-manifest.json`; this addendum supplies post-push final-head CI evidence without rewriting it. Scoped Go remains: typed protocol repair, existing Drafts and ordinary public PR/shadow CI. New private native execution, signing, publication and product qualification remain No-Go for the existing prerequisites and authorization limits.

Evidence prefix `workflow-runtime-final-8bbea4ab-ci-`: connector responses `runs-poll01.json`, `run-poll01.json`, `attempt-jobs-poll01.json`, `job-steps.json`, `job-logs.json`; decoded `contracts.log`; initial `summary.json` and corrected `final-summary.json`. All operations were harmless GitHub connector reads. No source edit, commit, push, dispatch, rerun, cancellation, settings, private/paid worker, signing or publication operation occurred.
