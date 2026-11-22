# QA309 public PR CI: read-only observation plan

This plan is derived from the four immutable pre-fix commits in `workflow-baseline.json`. Recheck the final committed workflow bytes before applying the expected job list. It describes observation only. It does not dispatch, rerun, cancel, approve an environment, build a new Runtime, sign, publish, or access private workers.

## Expected automatic work

| PR | Push to its existing Draft head | Required observation scope |
|---|---|---|
| Floorp #2889 | `colocated_runner_test.yml`, path-matching `qa3-source-shadow.yml`, and `docs_pipeline.yml` because the cumulative PR diff includes `.github/workflows/**` | Standard hosted smoke plus four Runtime asset jobs; Linux x64 browser suite; source shadow raw result; docs PR verify |
| Floorp #2893 | `colocated_runner_test.yml`; docs/qa-only changes do not match `docs_pipeline.yml` or `ppa_index_check.yml` | The same standard hosted smoke and four Runtime asset jobs, including Linux x64 browser suite |
| Runtime #83 | `qa3-shadow.yml` because the cumulative PR diff includes `.github/workflows/**` | Standard hosted Python contract step and policy preflight; no compiled native qualification |
| Runtime #84 | `qa3-shadow.yml` for the same reason | The same public contract step; verify the final #83 head is integrated into #84 |

There is no Draft exclusion in these test workflows. Unspecified `pull_request.types` uses opened/synchronize/reopened. Path matching applies to the PR's cumulative changed files, so a source-only follow-up push to #2889 still has the previously added workflow in its PR diff. A merge conflict or repository Actions setting can prevent execution; source eligibility is not proof that a run started. [GitHub event and path semantics](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request).

Runtime's existing push-native filters are `codex/runtime-variants/**/debug`, `.../pgo`, `.../recovery`, `.../mac-debug-recovery`, and `codex/runtime-build/**`. The heads `codex/qa3-shadow-foundation` and `codex/qa3-native-proof-contracts` match none. `qa3-linux-debug-proof.yml` is reusable-only, has no caller in either fixed repository workflow tree, and requires a private repository prerequisite. Source changes in these Drafts therefore do not dispatch the new private producer/consumer.

Floorp's existing `moderation.yml` is separate: only PR opened/edited, with its own model secrets and issue/PR write permissions. Updating a PR description can trigger it; a head synchronize alone does not. The standard `colocated_runner_test.yml` excludes its sole declared PAT metadata step on PR events, has contents-read permission, uses ordinary hosted labels, and has no signing/publishing/private proof edge. Do not generalize this to all PR workflows having no secrets.

## Final-head identity first

After authors commit and push, record each final PR head and base from a read-only API query and compare it to the locally reviewed commit. Keep the returned SHA, not just the branch name. For the stacked Runtime PRs, independently verify `git merge-base --is-ancestor NEW_83_SHA NEW_84_SHA` and #84's base metadata. If #83 advances later, repeat the integration check before concluding #84 is final.

```bash
qa309_repo='Floorp-Projects/Floorp'
qa309_pr='2889'
gh api --method GET "repos/$qa309_repo/pulls/$qa309_pr" \
  --jq '{number,state,draft,head:{ref:.head.ref,sha:.head.sha,repo:.head.repo.full_name},base:{ref:.base.ref,sha:.base.sha,repo:.base.repo.full_name},mergeable,mergeable_state}'
```

Repeat for Floorp #2893 and Floorp-Runtime #83/#84. A clean checkout and a successful local test before a subsequent edit do not prove that later head.

## Identify one exact run and attempt

```bash
qa309_final_sha='<reviewed and pushed full SHA>'
gh run list --repo "$qa309_repo" --event pull_request \
  --commit "$qa309_final_sha" --limit 100 \
  --json databaseId,workflowName,headSha,headBranch,event,status,conclusion,url,createdAt

qa309_run_id='<run ID from that listing>'
gh api --method GET "repos/$qa309_repo/actions/runs/$qa309_run_id" \
  --jq '{id,path,event,head_sha,head_branch,run_attempt,status,conclusion,html_url,repository:.repository.full_name,pull_requests:[.pull_requests[]|{number,head:.head.sha,base:.base.sha}]}'

qa309_attempt='<run_attempt from that exact run>'
gh api --method GET \
  "repos/$qa309_repo/actions/runs/$qa309_run_id/attempts/$qa309_attempt/jobs" \
  -f per_page=100 \
  --jq '{total_count,jobs:[.jobs[]|{id,name,run_id,head_sha,status,conclusion,labels,steps:[.steps[]|{name,number,status,conclusion}]}]}'
```

`--method GET` is explicit because adding fields to `gh api` otherwise changes its default method. Match repository, PR association, branch, final head, event and workflow **path**, not just display name. The jobs endpoint is attempt-specific. If `total_count` exceeds the returned jobs count, fetch the remaining pages before judging completeness. [GitHub workflow-job attempt endpoint](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt).

For PR workflows, checkout and `github.sha` normally identify the synthetic merge commit; API PR head identifies the head branch commit. The source receipt's `sourceHead` and `expectedSourceHead` must agree with the actual checked-out merge SHA, not be falsely rejected because they differ from the PR head SHA. Bind the run to the final PR head, and retain the checkout SHA from that run's checkout log or commit metadata. [GitHub merge-branch semantics](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#how-the-merge-branch-affects-your-workflow).

## Distinguish execution from a green summary

For `colocated_runner_test.yml`, require smoke and all four named matrix jobs, each completed/success. The expected matrix names are `Native Runtime / ubuntu-24.04`, `... / ubuntu-24.04-arm`, `... / windows-latest`, `... / macos-latest`. On Linux x64 require the combined browser suite, downloaded Firefox execution coverage, and runner unit-test steps to be completed/success. The other three matrix jobs intentionally skip Linux-only browser steps and must instead execute their locked native artifact validation. The smoke step `Validate locked Runtime release metadata` intentionally skips on PR events. These known step skips do not invalidate the job; a missing or skipped required job or required test step does.

Both QA3 shadow jobs have **job-level** `continue-on-error: true`. A failing shadow job can leave the workflow conclusion successful. Require the contract test step, source-plan/preflight step, and upload step each completed/success; retain the corresponding raw contract exit-code file (`0`), logs and receipt when available. Their API/UI success alone cannot prove the tests passed. [GitHub job continue-on-error semantics](https://docs.github.com/actions/reference/workflow-syntax-for-github-actions#jobsjob_idcontinue-on-error).

For a source shadow receipt, require `sourceVerdict=PASS`, fixed case coverage and run/attempt/checkout identity. `publicationAuthorized=false`, `productVerdict=UNVERIFIED` and `runtimeQualification=UNVERIFIED` remain constraints even when source tests pass. Runtime preflight is policy validation; it cannot become a native or product PASS. Source receipts or uploaded failure evidence existing after `always()` steps are not proof of success.

Record each scope independently as PASS, FAIL, pending, cancelled, or not run/not applicable. In-progress, queued, skipped, neutral, stale, timed_out, missing, cancelled, API error, wrong head, wrong workflow and wrong attempt are never executed PASS. A path-filtered absent workflow is not an executed PASS; it need not be a failure when that path scope is genuinely inapplicable. The ordinary CI success remains separate from private proof readiness and release authorization.

For bounded public logs, use the exact job ID and attempt; save the full small log before selecting excerpts. Artifact metadata can be observed without downloading browser binaries:

```bash
gh api --method GET "repos/$qa309_repo/actions/runs/$qa309_run_id/artifacts" \
  -f per_page=100 \
  --jq '{total_count,artifacts:[.artifacts[]|{id,name,size_in_bytes,digest,expired,workflow_run}]}'
```

Only inspect metadata-bound small public shadow receipts/logs with the existing bounded archive reader. Do not download an entire native bundle to establish a source shadow result. If logs/artifacts are unavailable, report that evidence gap instead of inferring PASS. `gh pr checks` and `gh run watch --exit-status` are convenient summaries, but do not replace head/attempt/job/step/raw-result checks for these nonblocking shadows.

## Existing publication gate scope

QA309 repair candidates must retain all existing workflow bytes, exact artifact transport IDs/digests and subject bindings, native signature verification needs, manual environment names and post-approval public-asset checks. A workflow source environment name preserves the gate declaration; repository reviewer/branch-protection settings were not queried here and must not be asserted fully enforced from source alone. The new required `source-once → final QA → manual approval → fresh sidecar revalidation → publish` path remains a future promotion requirement. It is not provided by a passing public shadow or by the preserved existing publisher.
