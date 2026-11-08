# FDH Gate 4 evidence (v0.1) — 2026-11-08

Jira: KAN-104 (epic), KAN-108 (4-A), KAN-109 (4-B), KAN-121 (4-C), KAN-122 (4-D).
Plan: Confluence 8061156 §3 (Gate 4). Decision record: 8061015 (D-06a/b/c, D-13).
Code at evidence time: main `27432ffc3b43d8167d0b7437067511fbd135fe3b` (PR #3075 merge); PR #3074 quota ledger merge `f449b6057da47e170783dd28837d6687d159cb3f`.

Gate 4 exit condition: **4-A, 4-B, 4-D pass; 4-C minimum resume.** This evidence records
**Gate 4 exit PASS** on 2026-11-08, with the caveats below.

## Verdict table

| item | verdict | evidence | notes |
| --- | --- | --- | --- |
| 4-A parallel Agents (AC-03) | **PASS** | `4a/` (core: agent1/2/3, safety: safety/) | 3 worktrees/offsets 0/100/200, parallel edit + host test + browser confirmation; stop isolation; GUI desktop lease; secret-reference denial; other-agent browser non-destruction. |
| 4-B regression set (AC-08, D-06b) | **PASS** | `4b/` | 6 merged fix PRs; 4 behavioral revert/fix pairs; 2 type-check-only; all 4 cases (known/fixed, explore→repro, not-reproduced with reason, infra failure) recorded; test code fixed. |
| 4-C local resume (AC-04 partial) | **PASS** | `4c/` + `4c/merged/` | resume restores state/worktree/base/offset and re-verifies; unknown push/PR waits for approval after PR #3074. |
| 4-D LLM quota ledger | **PASS** | `4d/` + `4d/merged/` | `fdh` manages claim/stop/resume/pending/approve/release/status; no double assignment; merged-code acceptance. |

## 4-A details

Core (`4a/agent{1,2,3}/`, summary `4a/core-summary.md`): three agents in separate worktrees at
offsets 0/100/200 edited `app-sidebar.tsx` with markers `QA4-A-AGENT1/2/3`, built, ran
`deno task test:host` (each `425 passed / 1 failed / 1 ignored`, the known pre-existing
`tools/src/initializer.test.ts:757`), started dev/browser, navigated a Fission content tab and
confirmed the marker, and built EvidenceManifest v0 (`state: succeeded`, assertions 2,
validation `[]`). Agent 1 stop closed its 33 processes and ports 2828/5183; Agents 2/3 stayed
listening (5280/5283, 5380/5383), re-verified, then stopped; all six ports closed.

Safety (`4a/safety/`): PR #3075 (`27432ffc`) adds the shared GUI-desktop lease at
`<git-common-dir>/fdh/gui-desktop-lease.json` and the secret-reference denial in `fdh evidence` /
`fdh secrets check`. Cross-worktree lease sequence: agent1 claim 0, agent2/3 claim 1, holder end 0,
agent2 claim 0, agent3 claim 1, agent2 cancel 0, agent3 claim 0, non-holder end 1, invalid reason 1,
final ended. With three browsers live, ending/stopping agent1 (33 processes) left agents 2/3 ports
and HTTP settings 200 intact; stopping agent2 left agent3 intact; agent3 end+down closed all.
Clean agent states: `denied:false, references:[], secretsFileRead:false`; a scratch state with
`.secrets` + `SIGNPATH_API_TOKEN` was denied by `secrets check` (2 markers) and by `fdh evidence`
(no manifest written). Merged-main re-check (`4a/safety/merged/`) repeats the lease/secrets contract
and PASSes.

## 4-B details

See `4b/report.md`. Six merged fix PRs: #3057 `6d6d4e54d807`, #2991 `1dcbf738be3e`,
#2984 `a218c5b9751d`, #2981 `f189ea878acf`, #2990 `a55a3c1a8413`, #2982 `374f987fbed9`.
Method: checkout merge commit; reverse only the PR source patch (`git apply -R`); run the PR's own
tests unchanged → expect failure; restore fix → same tests pass. Test files never edited.
Behavioral failures: 2991 (13 passed/3 failed), 2984 (16/4), 2990 (0/1), 2982 (20/3). Type-check
failures before assertions: 3057, 2981 (reverted tree removes modules/types the fixed tests import).
Four cases: (1) known/fixed pair PASS (#2991); (2) explore→repro PASS (#2982); (3) not reproduced
PASS, excluded with reason (#2981); (4) infra failure PASS (local `initializer.test.ts:757`).
No `inconclusive`-only pass.

## 4-C details

Existing `4c/report.md`: `fdh task resume` restored state/worktree/base/offset (`stale:[]`), dev up,
marker re-confirmation, evidence succeeded, dev down. `4c/merged/report.md`: after PR #3074 merge,
task `gate4-c-merged` recorded `quota pending --op push`, then `task resume` printed
`approvalPending:[push]` with restored fields, `quota approve` cleared it; resume never reruns
push/PR/release.

## 4-D details

`4d/` pre-merge CLI smoke (claim/stop/pending/resume/approve/status); `4d/merged/report.md` on
merged main: `gate4-d-merged` claim 0, second claim 1, non-holder stop 1, holder stop quota 0,
stopped claim 1, pending push 0, resume prints approvalPending, approve clears, resume agent-b 0,
release 0, status 0; ledger schemaVersion 1, events claim/stop/pending/approve/resume/release.

## Judgement axes (plan §4)

- **Job completion**: task states and evidence manifests record start/resume/stop/cleanup; 4-A
  agents completed edit→build→host test→browser→evidence; 4-B comparison jobs completed; 4-C/4-D
  CLI sequences completed with exit codes.
- **Product assertion**: 4-A markers confirmed in browser; host tests; 4-B fixed-test assertions;
  4-C resume re-verification. `matchesTarget:false` for the local packaged runtime.
- **Defect reproduction**: 4-B records `reproduced` (2991/2984/2990/2982 behavioral), type-check
  only (3057/2981), and the infra-failure classification; no inconclusive-only pass.

## Reviews

- PR #3074 (quota ledger): two independent LLM reviews (MiMo, DeepSeek) at fixed head
  `9955ae2e` — both `approve_with_notes`, blocking false; merged `f449b605`.
- PR #3075 (GUI lease/secrets): two independent LLM reviews (MiMo, DeepSeek) at head
  `aaf78bb` — both `approve_with_notes`, blocking false; merged `27432ffc`.
- 4-B uses already-merged fix PRs (#3057/#2991/#2984/#2981/#2990/#2982); their original PR reviews
  are not re-run here; the regression method fixes the test code and does not loosen criteria.
- Non-blocking notes carried: quota claim/GUI lease are read-modify-write without file locks
  (sequential single-driver safe; concurrent claims theoretical race); GUI lease is advisory with
  no forced release after a holder crash; secrets denial is record-based, not a filesystem ACL;
  docs generated at ancestor pins (front-matter churn only).

## Limitations

- Local macOS only; packaged Debug Runtime without `moz-src`; `FLOORP_SKIP_PATCHER=1` local-only
  guard (not in any PR tree); offset-200 runtime copied from offset-100; `matchesTarget:false`;
  host test is local; no visual judge; DSH pin UNVERIFIED; uncommitted diffs not hashed.
- 4-A safety acceptance ran at PR #3075 branch head and was re-checked on merged main; the core
  4-A run used main `bec27040` (before #3075).
- 4-B is a synthetic source-only reverse patch on a local worktree, not a reproduction on an
  affected user machine; two PRs fail by type checking, recorded honestly.
- 4-C merged extension is CLI-level (no dev up in that run); the earlier 4-C run covers dev/browser.
- 4-D verifies the ledger contract, not actual LLM token counting.
