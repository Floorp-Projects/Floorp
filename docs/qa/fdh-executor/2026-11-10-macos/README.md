# fdh job: B-02 test cases E-1..E-9 on macOS (2026-11-10)

Live run of the frozen B-02 ADR test cases
([Confluence 8716289](https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8716289),
decision D-23) on macOS (KAN-137). The Windows and Linux runs are in
[`../2026-11-10-windows/`](../2026-11-10-windows/README.md) and
[`../2026-11-10-linux/`](../2026-11-10-linux/README.md).

- Host: macOS 27.0 (`sw_vers -productVersion`) on Apple M5 Ultra (arm64, 30
  cores). This is the Mac Studio QA host, reached over Tailscale SSH. The
  `osRelease` in `summary.json` (27.0.0) is the Darwin kernel release, not the
  macOS version.
- Why not the macOS QA VM: the VM has no git, because the Command Line Tools are
  missing.
  - The VM normally runs with `--net-host` and cannot reach the internet. It was
    started again with NAT for the attempt, so the network could be worked
    around.
  - The Command Line Tools still could not be installed. They need 11.8 GB free,
    against 8.7 GB. Growing the disk would mean deleting the VM's Recovery
    partition, which was not done.
  - The run therefore used the host itself, and the VM was started again with
    its original `--net-host` settings.
- Deno 2.9.4: the official `deno-aarch64-apple-darwin.zip`, checked against the
  release's sha256, in a user directory. No system setting was changed.
- Code: a fresh clone at `main` 5b7829d8 (PR #3102), run as the normal user in
  its own directory. `summary.json` records the commit (`head`).
  `deno task test:host`: 594 passed / 0 failed, 1 ignored.
- Step: the real `deno task test:host` (about 3-5 s on this machine), started by
  `fdh job start --step test` with `--lease-ttl-ms 3000`.
- Command: `deno run -A tools/fdh_job_e2e.ts --out /tmp/fdh-mac-e2e-1`. The
  output was copied here unchanged; `run.log` is the console.

## Results

All cases passed on the first run: the nine ADR cases (E-4 and E-5 run as one
scenario) and E-2b, 31 checks.

| Case                     | Observed                                                                                                                                                                                                         |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| E-1 central stop         | The executor process was still alive a second after its controller was killed. A new controller reconnected, and the job `succeeded` with one attempt at epoch 1.                                                |
| E-2 runner stop          | Attempt 1 `lost`, epoch 2 `succeeded`. 2 processes of the live lost tree were stopped before epoch 2 started.                                                                                                    |
| E-2b interrupted respawn | Epoch 2 never started and was counted as lost, and epoch 3 `succeeded`. The epoch-1 step ended with exit 143 (SIGTERM) at 08:52:22.032. The only stop event (08:52:22.306) says 0 processes. See the note below. |
| E-3 ACK loss             | The outbox was applied by reconcile. Not rerun: one attempt.                                                                                                                                                     |
| E-4/E-5 comms loss       | Epoch 2 took over and `succeeded`. 2 processes were stopped before it started. The stalled epoch-1 report (exit 143) was rejected and recorded, and attempt 1 stays `lost`.                                      |
| E-6 unknown op           | `task resume` put the intent into `awaiting-approval` and the quota pending list. `job resolve` closed it and approved the entry.                                                                                |
| E-7 inputs changed       | `failed` (`input-changed`), no second attempt. 2 processes of the lost tree were stopped when the job closed.                                                                                                    |
| E-8 repeated loss        | `failed` (`gave-up`). 2 processes of each lost tree were stopped.                                                                                                                                                |
| E-9 report after loss    | The late confirm was refused and recorded (`late-report`).                                                                                                                                                       |

## Notes

- **The POSIX stop path stopped live trees on macOS too.** As on Linux, the step
  outlives its killed executor. E-2, E-4/E-5, E-7 and E-8 each record
  `lost-step-stopped` with 2 processes. In every respawn case the stop event
  comes before the next attempt started.
- **E-2b, read from the timestamps.** This is inferred; the events do not name
  the controller that wrote them.
  - The lease expired at 22.0. The epoch-1 step then ended with exit 143
    (SIGTERM) at 22.032. Source: `E-2b/outbox-epoch1.json`, the stalled
    executor's report. It is `results[0]` with `endedAt` 22.032 and `exitCode`
    143. The executor applied it at 33.5, after the journal was saved, and it
    was rejected as stale (`E-2b/executor-epoch1.log`). The journal alone does
    not show it. Both files were copied from the host's jobs directory after the
    run.
  - The 0-process stop event was written at 22.306.
  - This fits the interrupted path. The first controller sent the stop, which
    ended the step, and was killed before it recorded the stop. The next
    controller found the attempt not yet stopped, found nothing left to stop,
    and recorded 0. Epoch 2 had never started, and epoch 3 ran after that.
- No processes were left afterwards. `pgrep -u <user> -f .local/fdh-tools/deno`
  (every process of this Deno) and `pgrep -u <user> -f test:host` (the step)
  both found nothing.
- E-6 and E-9 take under a second here. As on Linux, probing the ports in
  `task resume` is fast.

## Limits

- One macOS machine (macOS 27.0, arm64). It is the host, not the QA VM (see
  above).
- In every case where processes were stopped, the step root was still alive.
  - In E-2b the second controller went through the root-gone selection, but
    nothing had survived for it to find.
  - The root-gone POSIX paths with surviving processes (orphans reparented to
    launchd, leaderless process groups) are not exercised. They remain in
    KAN-137.
- `--step build` is covered by unit tests only.
- The Deno binary and the clone stay on the host in user directories, to repeat
  the run: `~/.local/fdh-tools/deno` and `~/fdh-posix-e2e`.
