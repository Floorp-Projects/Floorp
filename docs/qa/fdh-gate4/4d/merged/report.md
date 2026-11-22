# 4-D LLM quota ledger — merged-code acceptance (re-captured)

Date: 2026-11-08 (JST)
Jira: KAN-122
Source tree: PR #3074 merge `f449b6057da47e170783dd28837d6687d159cb3f`, plus docs-only PR #3076 head `e77bbd4d8c9af51951d49c7d1025afed7db86f65` (no source changes; baseCommit recorded as e77bbd4d)
Worktree: `/tmp/fdh-gate4/evidence`
Task: `gate4-d-merged2`, offset 480 (ports 5880/5883, free)

## Steps and results (logs prefixed `d2-`)

| step | command | exit | observed |
| --- | --- | --- | --- |
| 1 | `task start --task-id gate4-d-merged2 --worktree ... --port-offset 480` | 0 | `d2-task-start.log` |
| 2 | `quota claim --agent agent-a` | 0 | activeAgent agent-a, events 1 |
| 3 | `quota claim --agent agent-b` | 1 | `already assigned to agent-a; release it before assigning another agent` |
| 4 | `quota stop --agent agent-b --reason quota` | 1 | `does not hold ... (holder is agent-a)` |
| 5 | `quota stop --agent agent-a --reason quota --note "LLM quota exhausted"` | 0 | stopped true, stopReason quota, activeAgent null |
| 6 | `quota claim --agent agent-b` | 1 | `is stopped; resume it before claiming` |
| 7 | `quota pending --op push` (no note) | 0 | approvalPending [{op:push, note:"", at}] |
| 8 | `task resume --worktree ...` | 0 | restored state-file/worktree/base-commit/port-offset, `stale:[]`, approvalPending [push] |
| 9 | `quota approve --note "human approved"` | 0 | approvalPending [] |
| 10 | `quota resume --agent agent-b` | 0 | activeAgent agent-b, stopped false |
| 11 | `quota release --agent agent-b` | 0 | activeAgent null, stopped false |
| 12 | `quota status` | 0 | schemaVersion 1, events 6, approvalPending [] |

First-party artifacts committed beside this report:
- `gate4-d-merged2-ledger.json` — schemaVersion 1, taskId gate4-d-merged2, worktree, stopped false at end, approvalPending [], events kinds claim/stop/pending/approve/resume/release (6).
- `gate4-d-merged2-task-state.json` — portOffset 480, events task start/quota claim/quota stop/quota pending/task resume/quota approve/quota resume/quota release (8).
- `d2-*.log` / `d2-*.err` for every step above.

## PASS / FAIL

- PASS: ledger managed by `fdh`, stop/resume/approval recorded, second active assignment rejected, stopped task cannot be claimed until resumed, pending external op waits for approval, resume reports approvalPending, release and status.

## Limitations

- Local-only CLI acceptance: it verifies the ledger contract, not LLM token counting itself.
- No dev server/browser started for this run; ports 5880/5883 were free (resume probe `stale:[]`).
- The source tree is PR #3074 merge plus the docs-only PR #3076 commit; no source/test change is in that commit, so the quota code equals merged main `f449b605`.
