# FDH Gate 4 — 4-C (AC-04 part) local resume acceptance (2026-11-08)

Worktree `/tmp/fdh-gate2/pr2b2`, task `gate4-a0` (offset 0, Marionette 2828, settings 5183),
base `bec27040382280c17a9e4a573359f017dca89036`. State was left by the 4-A isolation stop
(dev stopped, 33 processes gone, marker `QA4-A-AGENT1` uncommitted).

## Resume

`deno run -A tools/fdh.ts task resume --task-id gate4-a0` → exit 0:
```
{"taskId":"gate4-a0","worktree":"/private/tmp/fdh-gate2/pr2b2",
 "baseCommit":"bec27040382280c17a9e4a573359f017dca89036","portOffset":0,
 "status":"running","restored":["state-file","worktree","base-commit","port-offset"],
 "stale":[]}
```
Worktree, base commit, port offset and state file were restored; no stale entries.

## Restart + verify rerun

- `fdh dev up --task-id gate4-a0` → exit 0, `Browser ready! Marionette on port 2828.`
- `dev-tool navigate http://localhost:5183/ --context content` → exit 0, Title: Floorp Hub.
- `fdh verify browser` with the actor eval → exit 0,
  `QA4-A-AGENT1 @ http://localhost:5183/#/overview/home`.
- `fdh evidence` → `{"state":"succeeded","assertions":4,"limitations":13}`;
  manifest validates with role runner `[]`.
- `fdh dev down` → exit 0, `Stopped (33 processes)`; lsof 5183 empty.
- Marker remains uncommitted (`M app-sidebar.tsx`, `M tools/feles-build.ts`).

**Result: resume + verification rerun PASS for the local part of AC-04.**

## Not demonstrated

- Unknown non-idempotent external operations (push/PR) waiting for approval: the fdh CLI never
  pushes or opens PRs and `task resume` only restores local state/worktree/base/offset/ports; there
  is no approval-wait record for external operations in this CLI version. The 4-C pass condition
  "不明な非冪等外部操作は承認待ち" is therefore not yet demonstrated.

Evidence: `/tmp/fdh-gate4/acceptance/4c/` (resume.log, dev-up.log, navigate.log,
browser-verify.log, evidence-gate4-a0.json, dev-down.log, lsof, status).

Limitations: packaged runtime / `FLOORP_SKIP_PATCHER=1` guard / macOS local / no host test in
this step / visual judge absent / DSH pin UNVERIFIED / uncommitted diff not hashed.
