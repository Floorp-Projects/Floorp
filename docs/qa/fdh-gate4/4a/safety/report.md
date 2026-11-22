# 4-A safety conditions — GUI desktop lease, secret-reference denial, other-agent browser non-destruction

Date: 2026-11-08 (JST)
Jira: KAN-108 (AC-03)
Code: PR #3075 branch `codex/qa4-gui-secrets-20261108`, head `aaf78bbdd8099126522039b813e854c08f01faaf` (pre-merge acceptance)
Worktrees: `/tmp/fdh-gate2/pr2b2` (offset 0, task `gate4-a0`), `/tmp/fdh-gate2/pr2b100` (offset 100, task `gate4-a100`), `/tmp/fdh-gate4/pr2b200` (offset 200, task `gate4-a200`); all share git common dir `/tmp/fdh-gate2/pr2a-v2/.git`.

## 1. GUI desktop lease

- Lease path: `<git-common-dir>/fdh/gui-desktop-lease.json` — shared by every worktree of the repository, outside the tracked tree.
- Schema `schemaVersion:1`, `holder`, `taskId`, `worktree`, `acquiredAt`, `status: active|ended|cancelled`, `endedAt`, `events[]`.
- Claim rejects a second holder; only the current holder may end or cancel; non-holder end and invalid reason are rejected; `status` prints the shared lease.

CLI sequence (logs in this directory):

| step | command (cwd) | exit | observed |
| --- | --- | --- | --- |
| 1 | `gui claim --task-id gate4-a0 --agent agent1` (pr2b2) | 0 | active, holder agent1, task gate4-a0 |
| 2 | `gui claim --task-id gate4-a100 --agent agent2` (pr2b100) | 1 | `GUI desktop lease is held by agent1 for task gate4-a0` |
| 3 | `gui claim --task-id gate4-a200 --agent agent3` (pr2b200) | 1 | same holder rejection |
| 4 | `gui end --task-id gate4-a0 --agent agent1 --reason ended` | 0 | status ended, holder null |
| 5 | `gui claim --task-id gate4-a100 --agent agent2` | 0 | active, holder agent2 |
| 6 | `gui claim --task-id gate4-a200 --agent agent3` | 1 | held by agent2 |
| 7 | `gui end --task-id gate4-a100 --agent agent2 --reason cancelled` | 0 | status cancelled |
| 8 | `gui claim --task-id gate4-a200 --agent agent3` | 0 | active, holder agent3 |
| 9 | `gui end --task-id gate4-a0 --agent agent1` (non-holder) | 1 | `does not hold ... (holder is agent3)` |
| 10 | `gui end --task-id gate4-a200 --agent agent3 --reason bogus` | 1 | `--reason must be one of ended|cancelled` |
| 11 | `gui end --task-id gate4-a200 --agent agent3 --reason ended` | 0 | status ended, events 10 |
| 12 | `gui status --task-id gate4-a200` | 0 | ended, holder null |

Final lease copy: `lease-final.json` (status ended, 10 events). Live-lease sequence during browser checks: `lease-final-live.json` (status ended, 16 events).

## 2. Other-agent browser non-destruction (with lease)

Three dev/browser instances were started at offsets 0/100/200 (`dev-up-agent1/2/3.log`; ports 2828/5183, 5280/5283, 5380/5383; HTTP settings 200). Then:

| step | command | observed |
| --- | --- | --- |
| A claim | `gui claim ... agent1` | 0, active |
| B/C claim | agent2, agent3 | both 1, held by agent1 |
| A end | `gui end ... agent1 --reason ended` | 0; B/C 5280/5283/5380/5383 still listening, HTTP 200 (`live-b-*.txt`, `live-c-*.txt`) |
| A down | `dev down --task-id gate4-a0` | 0, `Stopped (33 processes)`; A 2828/5183 closed (`live-a-*-after.txt`), B/C ports still listening and HTTP 200 |
| B claim | agent2 | 0; C claim 1 held by agent2 |
| B cancel | agent2 cancelled | 0; C claim 0 |
| B down | `dev down --task-id gate4-a100` | 0, 33 processes; B ports closed, C 5380/5383 still listening and HTTP 200 |
| C end | agent3 ended | 0 |
| C down | `dev down --task-id gate4-a200` | 0, 33 processes; all six ports closed |

## 3. Secret-reference denial

- `fdh evidence` refuses a task state that references `.secrets` or its token names; `fdh secrets check` exposes the same check; fdh scans only task records and never reads `.secrets`.
- Clean agent task states: `gate4-a0`, `gate4-a100`, `gate4-a200` → `{denied:false, references:[], secretsFileRead:false}` (exit 0; `secrets-agent{1,2,3}.log`).
- Deliberate scratch task `gate4-a-secret-denial` with `.secrets` + `SIGNPATH_API_TOKEN` in an event note → `denied:true`, references 2 (`$.events[1].note` -> `.secrets`; `$.events[1].note` -> `SIGNPATH_API_TOKEN`), `secrets check` exit 1, `fdh evidence` exit 1 with no manifest written (`secrets-denied.*`, `evidence-denied.*`).

## PASS / FAIL

- PASS: lease exclusivity across three worktrees, cancel/end by holder only, non-holder and invalid-reason rejection, final release.
- PASS: ending/stopping Agent A (33 processes) did not stop Agent B/C browsers (ports + HTTP 200); stopping B did not stop C.
- PASS: three agent task records have no secret references; a record with `.secrets`/token references is denied by `secrets check` and by `fdh evidence`.

## Limitations

- Local macOS only; packaged Debug Runtime without `moz-src`; `FLOORP_SKIP_PATCHER=1` local-only guard; runtime prepared by hand for offsets 0/100/200 (offset 200 copied from 100).
- The lease serializes the shared desktop GUI; parallel edits/builds/host tests are covered by the 4-A core evidence (`/tmp/fdh-gate4/acceptance/4a-agent{1,2,3}/`), not re-run here.
- Secret denial is on evidence/task records and on fdh never reading `.secrets`; it is not a filesystem ACL for other processes.
- Acceptance ran at PR #3075 branch head (pre-merge); `matchesTarget:false`; no host test in this run; visual judge none.
