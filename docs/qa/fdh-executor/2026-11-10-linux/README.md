# fdh job: B-02 test cases E-1..E-9 on Linux (2026-11-10)

Live run of the frozen B-02 ADR test cases
([Confluence 8716289](https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8716289),
decision D-23) on a POSIX machine (KAN-137). The Windows run is in
[`../2026-11-10-windows/`](../2026-11-10-windows/README.md).

- Host: Debian GNU/Linux 13 (trixie), kernel 6.12.111, x86_64, 4 cores (the QA
  desktop reached over Tailscale SSH), Deno 2.8.3.
- Code: a fresh clone at `main` 2726e719 (PR #3097), run as an unprivileged user
  in its own directory. `deno task test:host`: 594 passed / 0 failed, 1 ignored.
- Step: the real `deno task test:host` (about 6-10 s), started by
  `fdh job start
  --step test` with `--lease-ttl-ms 3000`.
- Command: `deno run -A tools/fdh_job_e2e.ts --out /tmp/fdh-posix-e2e-1`. The
  output was copied here unchanged; `run.log` is the console.
- Output: `summary.json` holds every check. Each case has the job journal
  (`journal.json`) and the task-state events (`task-events.json`).

## Results

All cases passed on the first run: the nine ADR cases (E-4 and E-5 run as one
scenario) and E-2b, 31 checks.

| Case                     | Observed                                                                                                                                                                   |
| ------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| E-1 central stop         | The executor process was still alive a second after its controller was killed. A new `job wait` reconnected, and the job `succeeded` with one attempt at epoch 1.          |
| E-2 runner stop          | Attempt 1 `lost`, epoch 2 `succeeded`. The controller stopped 2 processes of the lost step tree, and the lost root is gone.                                                |
| E-2b interrupted respawn | The controller was killed mid-respawn. Epoch 2 never started and was counted as lost. The next controller stopped epoch 1's tree (2 processes), and epoch 3 `succeeded`.   |
| E-3 ACK loss             | The outbox was applied by reconcile (`outbox-applied`). Not rerun: one attempt.                                                                                            |
| E-4/E-5 comms loss       | Epoch 2 took over and `succeeded`. The stalled epoch-1 report was rejected and recorded once (`stale-epoch`), and attempt 1 stays `lost`.                                  |
| E-6 unknown op           | `task resume` put the intent into `awaiting-approval` and the quota pending list. A second resume did not re-queue it, and `job resolve` closed it and approved the entry. |
| E-7 inputs changed       | `failed` (`input-changed`), no second attempt. The lost tree (2 processes) was stopped when the job closed.                                                                |
| E-8 repeated loss        | `failed` (`gave-up`) after two lost attempts. Both lost trees (2 processes each) were stopped.                                                                             |
| E-9 report after loss    | The late confirm was refused and recorded (`late-report`). The job still awaits approval.                                                                                  |

## Differences from the Windows run

- **The step outlives its killed executor on Linux.** On Windows, killing the
  executor closed the step's output pipes, and its tree had exited before any
  controller looked. On Linux the step kept running in its own process group
  (detached spawn). So the POSIX stop path did the work in every loss case: E-2,
  E-2b, E-4/E-5, E-7 and E-8 each record `lost-step-stopped` with 2 processes.
  This is the path that was untested before.
- After the run, no processes from the run's directory were left
  (`pgrep -u user -f fdh-posix-e2e`).
- E-6 and E-9 take about one second here, against about 45 s on Windows. Probing
  the recorded ports in `task resume` is fast on Linux.

## Limits

- One Linux machine (x86_64, Debian 13). macOS has not been run yet.
- In every loss case the step root was still alive when the controller stopped
  the tree. These runs therefore do not exercise the root-gone paths on POSIX:
  orphans reparented to init, and leaderless process groups. Both are listed in
  KAN-137.
- `--step build` (multi-command checkpoints) is covered by unit tests only.
- Deno 2.8.3 here and 2.9.4 on Windows. Both pass the same host tests.
