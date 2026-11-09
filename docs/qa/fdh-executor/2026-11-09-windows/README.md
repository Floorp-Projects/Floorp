# fdh job: B-02 test cases E-1..E-9 on Windows (2026-11-09)

Live run of the frozen B-02 ADR test cases
([Confluence 8716289](https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8716289),
decision D-23) against real processes, for AC-04 on one machine.

- Host: Windows 11 Pro 10.0.26300, Deno 2.9.4, this worktree.
- Step: the real `deno task test:host` (about 5-10 s), started by
  `fdh job
  start --step test` with `--lease-ttl-ms 3000`.
- Command:
  `deno run -A tools/fdh_job_e2e.ts --out docs/qa/fdh-executor/2026-11-09-windows`
- Output: `run.log` (console), `summary.json` (every check), and per case the
  job journal (`journal.json`) and the task-state events (`task-events.json`).

Faults are injected by killing real processes (`Deno.kill(pid, "SIGKILL")`,
TerminateProcess on Windows). Two crash windows cannot be hit by timing and use
the executor's test-only `FDH_JOB_FAULT`: `exit-after-outbox` (E-3) and
`stall:<ms>` (E-4/E-5).

## Results

All nine cases passed (28 checks).

| Case                          | What was done                                                                    | Observed                                                                                                                                                           |
| ----------------------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| E-1 central stop              | `job wait` (the controller) killed while the step ran; a new `job wait` started  | Executor kept running; the new controller reconnected; `succeeded` with one attempt at epoch 1                                                                     |
| E-2 runner stop               | Executor killed while the step ran                                               | Lease expired; attempt 1 `lost`, epoch 2 started and `succeeded`; the lost attempt's step process is gone                                                          |
| E-3 ACK loss                  | Executor exits after writing the outbox, inside the journal lock, before the ACK | Journal still `running`; reconcile applied the outbox (`outbox-applied`); not rerun (one attempt)                                                                  |
| E-4 comms loss                | Executor stops touching the journal for 12 s (TTL 3 s)                           | Epoch 2 took over and `succeeded`                                                                                                                                  |
| E-5 old epoch                 | The stalled epoch-1 executor reports after the takeover                          | Report rejected and recorded (`stale-epoch` at epoch 1); attempt 1 stays `lost` with the late report in its notes; the job's result is epoch 2's                   |
| E-6 unknown non-idempotent op | `job intent --op push`, then `task resume` (holder gone)                         | `awaiting-approval` and a quota pending entry; a second resume did not rerun or re-queue it; `job resolve --resolution succeeded` closed it and approved the entry |
| E-7 inputs changed            | Executor killed, then an untracked file added                                    | Job `failed` (`input-changed`); no second attempt                                                                                                                  |
| E-8 repeated loss             | `--max-attempts 2`; executors of epoch 1 and 2 killed                            | Job `failed` (`gave-up`); two attempts, both `lost`                                                                                                                |
| E-9 report after loss         | Intent's holder declared lost by `task resume`, then `job confirm --epoch 1`     | Confirm refused; still `awaiting-approval`; `late-report` recorded                                                                                                 |

Pass rule (ADR section 6): E-2, E-3 and E-4 reached `succeeded`; only E-6 (and
E-9, which is E-6 plus a late confirm) waited for approval. Nothing passed by
holding everything.

## Notes

- Stopping a lost step tree: before starting the next epoch, the controller
  stops the lost attempt's step process tree, selected by pid, parent pid and
  start time and identity-checked before anything is stopped. The journals
  record it as `lost-step-stopped`.
  - In E-4/E-5 the stalled executor was alive. Three processes of its step tree
    were stopped before epoch 2 ran.
  - In E-2 and E-8 the step tree had already exited by the time the controller
    looked, and 0 processes were stopped. Killing the executor closes the step's
    output pipes. E-2 checks that the lost step's root process is not running.
- The lease TTL is 3 s here to keep the run short. The default is 10 s.
- E-6 takes about 45 s, mostly in the two `task resume` calls, which probe the
  recorded ports.

## Limits

- One machine only: no central scheduler and no cross-machine lease (ADR section
  7). exactly-once is not claimed. In E-2 and E-4 the safe step ran again.
- Windows only in this run. The POSIX process-group path (`stopProcessTree`
  signals `-pid`) is not exercised here.
- `--step build` (multi-command checkpoints) is covered by unit tests
  (`tools/src/fdh_executor.test.ts`), not by this live run, because a source
  build takes minutes and needs the Runtime.
- Intents and confirms are self-reported by the agent, and the run uses the
  agent name `agent-a`.
