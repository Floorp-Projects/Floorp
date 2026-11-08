# 4-D LLM quota ledger — merged-code acceptance

Date: 2026-11-08 (JST)
Jira: KAN-122
Code: main `f449b6057da47e170783dd28837d6687d159cb3f` (PR #3074 merge), worktree `/tmp/fdh-gate4/quota`
Task: `gate4-d-merged`, offset 400 (ports 5480/5483, free)

## Steps and results

| step | command | exit | observed |
| --- | --- | --- | --- |
| 1 | `task start --task-id gate4-d-merged --worktree ... --port-offset 400` | 0 | state written, baseCommit f449b605 |
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

Ledger file: `/tmp/fdh-gate4/quota/_dist/fdh/quota/gate4-d-merged.json` — schemaVersion 1, events kinds claim/stop/pending/approve/resume/release, no double assignment.
Task state events include one event per mutating quota op; `task resume` recorded once (exit 0).

## PASS / FAIL

- PASS: ledger managed by `fdh`, stop/resume/approval recorded, second active assignment rejected, stopped task cannot be claimed until resumed, pending external op waits for approval, resume reports approvalPending, release and status.

## Limitations

- Local-only CLI acceptance: it verifies the ledger contract, not LLM token counting itself.
- No dev server/browser started for this run; ports 5480/5483 were free (resume probe `stale:[]`).
