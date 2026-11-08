# FDH Gate 4 — 4-A (AC-03) core parallel-agent acceptance (2026-11-08)

Base main: `bec27040382280c17a9e4a573359f017dca89036` (Gate 3 evidence merged).
Worktrees / offsets / ports:
- Agent 1: `/tmp/fdh-gate2/pr2b2`, offset 0, Marionette 2828, settings 5183
- Agent 2: `/tmp/fdh-gate2/pr2b100`, offset 100, Marionette 5280, settings 5283
- Agent 3: `/tmp/fdh-gate4/pr2b200`, offset 200, Marionette 5380, settings 5383

Offset 200 runtime: copy of the hand-prepared offset-100 packaged runtime; smoke-tested
(`Browser ready! Marionette on port 5380`, settings 5383 page loaded via content navigate,
dev down stopped 32 processes).

**Result: core AC-03 PASS.** Three DSH agents ran in parallel in separate worktrees/offsets:
each did task start → small uncommitted edit (`QA4-A-AGENT1/2/3` on
`app-sidebar.tsx:133`) → build (`feles-build before-mach`, exit 0) → dev up at its offset →
content navigate → browser verify via Marionette actor eval → `deno task test:host` →
2-D evidence. All three confirmed their own marker:
- `QA4-A-AGENT1 @ http://localhost:5183/#/overview/home`
- `QA4-A-AGENT2 @ http://localhost:5283/#/overview/home`
- `QA4-A-AGENT3 @ http://localhost:5383/#/overview/home`

Host test (all three, honestly): `FAILED | 425 passed | 1 failed | 1 ignored`, the single
failure the pre-existing `tools/src/initializer.test.ts:757` locked Runtime install test
(actual 000 vs expected 002) — not caused by the marker edit.

Evidence (2-D, role runner validated `[]`): each agent `state: succeeded`, assertions 2
(browser-connected, browser-eval-contains), limitations 12–13.

## Isolation test (one stops, others continue)

- Agent 1 (`pr2b2`) `fdh dev down`: exit 0, `Stopped (33 processes)`; lsof 5183 and 2828 empty.
- Immediately after, Agent 2 and 3 were still running: lsof 5283/5280 and 5383/5380 listening;
  lsof 5183/2828 empty. Both re-verified in the still-running browser:
  `QA4-A-AGENT2 @ http://localhost:5283/#/overview/home`,
  `QA4-A-AGENT3 @ http://localhost:5383/#/overview/home`, exit 0.
- Then Agent 2 and 3 stopped their own dev: 5283/5280 and 5383/5380 closed; all six ports
  (5183/5283/5383/2828/5280/5380) empty at the end.

Evidence: `/tmp/fdh-gate4/acceptance/4a-agent{1,2,3}/` (reports, logs, manifests, states,
lsof before/after, isolation-stop.md, isolation-reverify.md).

## Not yet closed (4-A remaining conditions)

- GUI Desktop lease (add + verify).
- Secrets reference denial.
- Other user's browser non-destruction (beyond the three agent browsers/profiles above).
These are required by the 4-A pass condition "GUI/Secrets条件を満たす" and are not yet
implemented/verified.

## Limitations

- Local packaged Debug Runtime without `moz-src`; local `FLOORP_SKIP_PATCHER=1` guard;
  `omni.ja`/dev CSP/`noraneko-devdir` hand-prepared; macOS local only; `matchesTarget: false`;
  no visual judge; DSH pin UNVERIFIED; uncommitted marker diffs not hashed in manifests.
- Offset-200 runtime is a copy of the offset-100 prepared runtime (port-independent browser.xhtml
  CSP; prefs written per worktree), smoke-tested once before the parallel run.
- 2-D manifests for 4-A have no build assertion because `feles-build build --phase before-mach`
  was run outside the fdh CLI; build exit 0 is recorded in logs/reports.
