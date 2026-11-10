# fdh ext: live AC-06 run on KAN-138 (2026-11-11)

This is the live test of the strengthened AC-06 flow for KAN-131 (decision
D-19):

> Issue investigation → independent branch → Draft PR → review by separate
> agents → CI → Jira update

The test also exercises duplicate events, ordering inversion and concurrent
edits. Every operation that fdh plans or keys went through `fdh ext`, from the
worktree, with the code of each step. These were creating the PR, marking it
ready, merging it, and the Jira comments and transitions.

- The agent read GitHub with `fdh ext poll` (`gh api`) and read Jira itself
  through Atlassian MCP.
- fdh planned each change (`ext plan`).
- The agent performed the change with `gh` or Atlassian MCP, inside a keyed
  intent (`ext start` … `ext done`).

Some external writes were not fdh operations:

- **Outside fdh by design:** creating KAN-138, the `git push` of the branch and
  of the review fix, and the two D-14 receipt comments. The receipts are not a
  synced field.
- **Outside fdh on purpose:** the Jira transition back to 未着手 that stood in
  for a person's edit.

## Subject

- **Issue:** KAN-138, created for this run. It records the investigation:
  - Atlassian MCP cannot create Jira remote links, so the `pr.url` → remote-link
    rule could not be carried out by an agent.
  - Two notes left over from #3118: credentials in ssh remotes, and no `gh`
    timeout.
- **Branch and PR:** `fdh/kan-138` → PR #3119, squash-merged as 6fe7bc6d. The PR
  changes the `pr.url` mirror to a keyed-once Jira comment, hides ssh
  credentials, and times out `gh`.
- **Task and ledger:** task `ac06-kan138`; the ledger is `ext-ledger.json`. The
  intent journals are in `jobs/`, and the step-by-step console is in `run.log`.

Two caveats about the record:

- **Code changed during the run.** The run started on `main` e84a10f7, and the
  plan at 18:36:31Z still printed the old `remote-link` row. The branch commit
  c8e265fa (18:38:10Z) was the PR's first head. The review fix 1d687fb1
  (18:43:17Z) followed.
- **One step is missing from the log.** The first
  `ext link --branch fdh/kan-138` ran before `run.log` was started.

## What happened

All times are UTC.

| Step                        | Observed                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Duplicate PR prevention     | `ext start github:pr-create:fdh/kan-138` → `gh pr create --draft` (#3119) → `ext done`. A second `ext start` of the same key, under another agent name (`claude-other`, the same session), returned `already-done`.                                                                                                                                                                                                                                                                 |
| First poll and plan (18:38) | `ext poll` gave `pr.state=draft` and `ci.result=<sha>:pending`. The plan had two actions: the PR-link comment, and status → 進行中. The pending CI result was not posted.                                                                                                                                                                                                                                                                                                           |
| Unknown outcome (502)       | The Jira comment call returned `502 Bad Gateway`. The agent checked with `getJiraIssue`: there was no comment and `updated` was unchanged. It then recorded `ext done --outcome failed` and started the same key again (a failed key may be retried). The retry posted comment 10756. The read at the end of this file shows exactly one PR-link comment.                                                                                                                           |
| Write, then poll            | Status → 進行中 was performed and recorded, and the plan showed `awaiting-poll`. A Jira poll whose `updated` (03:39:11.063+09:00) is earlier than `ext done` brought it to in sync.                                                                                                                                                                                                                                                                                                 |
| Duplicate and inversion     | Replaying the same event id gave `duplicate`, and the earlier 未着手 delivered late gave `stale`. Neither was taken.                                                                                                                                                                                                                                                                                                                                                                |
| Concurrent edit             | Acting as a person, the agent moved KAN-138 back to 未着手 while the PR stayed a draft: the plan kept the edit (`keptEdits`) and wrote nothing. The PR was then marked ready (`github:pr-ready:3119`), so both sides had changed since fdh's write: the plan held `pr.state` as `concurrent-edit`. Playing the approver, the agent chose the PR side (`ext resolve --keep owner`). The new key `KAN-138:pr.state:レビュー中#2` was performed and polled, and the field was in sync. |
| Review by separate agents   | Two reviewer agents, separate from the agent that did the work, approved 1d687fb1: reviewer A on claude-fable-5-2 and reviewer B on claude-opus-5-5. Their D-14 receipts were posted on #3119; the receipt for reviewer A wrongly named claude-fable-5-1, and a correction was posted on the PR.                                                                                                                                                                                    |
| CI                          | The aggregated check-run result was success (`gh pr checks` also showed every check passing). The poll gave `<sha>:success`, and the plan gave one keyed-once CI comment (10757), which was posted.                                                                                                                                                                                                                                                                                 |
| Merge and Jira update       | `github:pr-merge:3119` was recorded and the squash merge landed (6fe7bc6d). The poll gave `merged`, and the plan gave `完了#3`, which was performed. The final plan has no actions and no holds; `pr.url`, `pr.state` and `ci.result` are in sync.                                                                                                                                                                                                                                  |

The 9 intents are in `jobs/`: 8 `succeeded` and 1 `failed`, the comment attempt
that hit the 502.

## What this shows, and what it does not

**Shown live:**

- The whole flow, recorded by fdh:
  - Issue → branch → Draft PR → separate-agent review → CI → merge → Jira 完了.
- Duplicate-PR prevention by key.
- An unknown outcome handled the way SKILL.md says: check, record `failed`,
  retry. The ledger keeps only the retried op for the key; the failed attempt is
  in its journal and in `run.log`.
- The poll-after-write wait.
- A duplicate event and an ordering inversion, both rejected.
- A concurrent edit, held and then settled by an approver.

**Not shown here:**

- **A real webhook redelivery.** D-19 has no webhook; the duplicate and the
  inversion were replays fed to `ext observe`.
- **An edit by a real second person.** The concurrent edit was made by the agent
  on purpose.
- **A lost agent sent to `awaiting-approval`.** This is covered by the B-02 runs
  (E-6, E-9) and by unit tests.
- **A real second agent or a human approver.** The second `ext start` and the
  `ext resolve` decision were made by the same session.
- **Paths that never came up.** There was no CI failure comment, no deletion and
  no `mirror-unobserved` hold.
- **Live Jira events.** The Jira observations, including the replayed event id,
  were built by hand from Atlassian MCP reads.

KAN-138 was created for this run, so no one else edited it.

## Final read-only check

A read-only Atlassian MCP `getJiraIssue` read was made after the run, at about
19:05Z, before this file was committed. It showed:

- KAN-138's status is 完了, `updated` 2026-11-11T03:55:27.622+09:00.
- KAN-138 has exactly two comments, both by fdh:
  - 10756, the PR link, created 03:39:03.725+09:00;
  - 10757, CI success on 1d687fb1, created 03:54:51.277+09:00.

On GitHub, PR #3119 is merged as 6fe7bc6d.
