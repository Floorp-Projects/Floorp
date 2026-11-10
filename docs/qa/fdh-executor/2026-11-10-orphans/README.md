# fdh job: orphans of a lost step (E-2c) on Linux, macOS and Windows (2026-11-10)

KAN-137 follow-up to the B-02 runs in
[`../2026-11-10-linux/`](../2026-11-10-linux/README.md),
[`../2026-11-10-macos/`](../2026-11-10-macos/README.md) and
[`../2026-11-10-windows/`](../2026-11-10-windows/README.md). Those runs kill
only the executor, so the step's root process stays alive and its tree is found
by parent pid. E-2c also kills the step's root (`deno task test:host`). Its
child (`deno test`) then outlives it:

- On POSIX the child is re-parented to init (parent pid 1).
- On Windows it ends with the root here.

The takeover must stop that orphan before epoch 2 runs the step again.

## Runs

| Directory     | Host                                                     | Deno  | Code                                                                                                                                             |
| ------------- | -------------------------------------------------------- | ----- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `linux/`      | Debian 13, kernel 6.12.111, x86_64 (QA desktop over SSH) | 2.8.3 | branch `fdh/posix-orphans` f2a82803, all cases                                                                                                   |
| `macos/`      | macOS 27.0 (Mac Studio host, Darwin 27.0.0), arm64       | 2.9.4 | branch f2a82803, all cases                                                                                                                       |
| `windows/`    | Windows 11 Pro 10.0.26300, x86_64                        | 2.9.4 | branch f2a82803, all cases                                                                                                                       |
| `*/baseline/` | the same Linux and macOS hosts                           | —     | f2a82803 with `tools/src/fdh_job_process.ts` and `tools/src/fdh_job.test.ts` taken from `main` e011353d (the code before this change), E-2c only |

How the runs were made:

- **Command:** `deno run -A tools/fdh_job_e2e.ts --out <dir> [--only E-2c]`.
- **Output:** copied here unchanged.
  - `run.log` is the console output.
  - `summary.json` holds every check. Its `head` is the checked-out commit, so
    for the baselines it does not show the two files taken from `main`.
  - Each case directory has the job journal and the task-state events.
- **Baseline method:** the baseline swaps in `main`'s test file as well.
  Otherwise the step (`deno task test:host`) fails type-checking within a
  second, before it reaches the point E-2c waits for.

## Results

| Run               | E-2c                                                                                                                                                                       | Other cases                        |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| `linux/`          | PASS. The orphan (pid 913199, parent pid now 1) was stopped by the takeover reconcile: `stopped 1 process(es) of the step tree rooted at pid 913191`. Epoch 2 `succeeded`. | E-1..E-9 and E-2b PASS (35 checks) |
| `macos/`          | PASS. The orphan (pid 6356, parent pid now 1) was stopped by the takeover: `stopped 1 process(es) …`. Epoch 2 `succeeded`.                                                 | E-1..E-9 and E-2b PASS (35 checks) |
| `windows/`        | PASS. No orphan was left: both children of `deno task` were gone 0.5 s after the root was killed. The takeover stopped 0 processes, and epoch 2 `succeeded`.               | E-1..E-9 and E-2b PASS (34 checks) |
| `linux/baseline/` | **FAIL as expected.** The orphan (pid 915969, parent pid 1) was still running after the takeover. The stop counted 0 processes, so epoch 2 ran the step beside it.         | not run                            |
| `macos/baseline/` | **FAIL as expected.** The orphan (pid 6690, parent pid 1) was still running after the takeover. The stop counted 0 processes.                                              | not run                            |

The baselines show the gap this change closes. Once the root was gone, the code
on `main` looked for the lost tree only by parent pid. A re-parented child has
parent pid 1, so nothing was selected.

The branch adds a second way to find it, through the process group:

- The step command leads its own group.
- The orphan stays in that group.
- A group id is not reused while the group has members.
- The member must also have started between the root and the bound.

## Not covered

- **A group member that started after the bound.** The bound is the last
  heartbeat plus one TTL, or the time the loss was declared. Such a member is
  not trusted and so not stopped. The unit tests cover this.
- **Members that left the group or started their own session** (`setsid`). These
  are found only through their parent pid. A daemon that double-forks out of the
  group is not found once its parent has exited.
- **On Windows, an orphan that keeps the dead parent's pid.** That case is the
  existing path, already covered by the unit tests. It could not be produced
  live here, because `deno task`'s children end with it.
