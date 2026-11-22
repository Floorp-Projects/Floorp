# 4-C local resume — approval-pending extension (merged code)

Date: 2026-11-08 (JST)
Jira: KAN-121
Code: main `f449b6057da47e170783dd28837d6687d159cb3f` (PR #3074 merge), worktree `/tmp/fdh-gate4/quota`
Task: `gate4-c-merged`, offset 420 (ports 5680/5683, free)

This extends the existing 4-C resume evidence (`/tmp/fdh-gate4/acceptance/4c/report.md`, which restored state/worktree/base/offset, ran `stale:[]`, dev up, marker re-confirmation, evidence, dev down) with the approval-wait path now implemented by PR #3074.

## Steps and results

| step | command | exit | observed |
| --- | --- | --- | --- |
| 1 | `task start --task-id gate4-c-merged --port-offset 420` | 0 | state written, baseCommit f449b605 |
| 2 | `quota claim --agent agent-a` | 0 | holder agent-a |
| 3 | `quota pending --op push --note "remote state unknown; do not retry until approved"` | 0 | approvalPending [push] |
| 4 | `task resume --worktree ...` (simulated restart) | 0 | restored state-file/worktree/base-commit/port-offset, `stale:[]`, `approvalPending:[push]` printed |
| 5 | `quota approve --note "human approved retry"` | 0 | approvalPending [] |
| 6 | `quota status` | 0 | events claim/pending/approve; approvalPending [] |

Task state: events `task start`, `quota claim`, `quota pending`, `task resume`, `quota approve`; resume never reruns push/PR/release (fdh resume does git cat-file + TCP probe only).

## PASS / FAIL

- PASS: resume restores worktree/base/offset/records after restart and reports the unknown non-idempotent external op as awaiting approval; approval clears it before retry.

## Limitations

- Local-only CLI resume; the earlier 4-C run covered dev-up/marker/evidence/browser after resume.
- No dev server/browser started in this extension; ports 5680/5683 were free.
