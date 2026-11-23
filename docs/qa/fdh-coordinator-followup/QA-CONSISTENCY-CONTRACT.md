# Final-package QA consistency contract (prepared, not a publication gate)

## Scope
The three new files under `tools/src/fdh_final_package_qa_gate*` implement a pure comparison of a trusted expectation and unknown caller-observed REST/decoded-evidence data. They perform no filesystem, network, archive extraction, hashing, native QA or publication operation. The public function is `validateFinalPackageQaConsistency`.

Every successful result permanently contains:

```json
{
  "attestationConsistencyMatched": true,
  "archiveBytesVerified": false,
  "publicationAuthorized": false,
  "scope": "source-attestation-consistency-only"
}
```

This result MUST NOT be used alone as a publisher dependency or proof of QA qualification. A matching caller-provided digest string is not byte verification. “Signed mode” means `unsigned:false` for the package workflow; it does not mean the JSON attestation has a verified cryptographic signature. QA3 remains canonical for QA policy.

## Authoritative schema consulted
- Current Confluence decisions were re-read: https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8061015 remains version 12 (D-15 through D-23 unchanged).
- Existing workflow emits `input.json` with package run/head, source versions, probe head, target, unsigned flag, selected package/record references and source eligibility.
- `source-after-qa.json` contains only `sourceEligibility` and `checkedAt`. No imaginary QA run/head/attempt fields were added to this file. Its enclosing artifact metadata supplies those relationships.
- Read-only API inspection of QA run https://github.com/Floorp-Projects/Floorp/actions/runs/43363023598 confirmed job `head_sha`, `head_branch`, `run_id` and `run_attempt` exist. Artifact `workflow_run` supplies run/head/branch/repository IDs, but no artifact attempt field. Historical metadata inspection is not a new live gate test.

## Checked consistency
- Canonical repository, workflow paths/IDs, package and QA run IDs/heads/branches/current attempts and expected states.
- Exact four canonical QA jobs; missing, duplicate, skipped, cancelled and unexpected mandatory failures rejected. Job run/attempt/head/branch must agree.
- Exact selected artifact IDs/names/digest claims, source run/head/repository/head-repository/branch, expiration and conservative current-attempt time windows. Expired duplicates still count as ambiguity.
- Both required pre/post files, duplicate/path ambiguity checks, signed-package-mode target/source/probe/version metadata and package/record/bundle references.
- Pending package `updated_at` is not an upload-completion boundary: package artifacts are bounded by QA intake time instead.
- API upper ordering timestamps preserve their declared precision: seconds cover through .999, one/two/three fractional digits cover 100/10/1 ms intervals. This is interval consistency, not arbitrary clock-skew tolerance. Trusted `now`, freshness and attempt lower bounds remain strict. Raw API precision must not be normalized away by the future collector.
- Windows policy must be explicitly supplied by the trusted caller. Shadow is limited observation, not all-OS qualification. Missing post-QA evidence is rejected even in shadow mode; synthetic complete failure shapes do not establish live reachability of that path.

## Verification

```sh
deno test --cached-only --frozen tools/src/fdh_final_package_qa_gate.test.ts
deno lint tools/src/fdh_final_package_qa_gate.ts tools/src/fdh_final_package_qa_gate_types.ts tools/src/fdh_final_package_qa_gate.test.ts
node --test tools/test-final-package-qa-source.mjs
```

- Repository regression test file (local uncommitted candidate): 85/85 cases pass without permission grants.
- Independent reviewer additionally ran 30 in-memory adversarial probes; these are separate review experiments, not 30 extra committed test cases.
- Existing source intake baseline: 28/28 pass and its implementation is unchanged.
- Full canonical host suite recorded by FDH: 831 passed / 0 failed / 3 ignored.
- Exact independently reviewed SHA-256: helper `C85342AFA8F7A143C11D7A1141BAF7934C8988BF6375C7F3E8344C5D4853CAD6`; types `838FFA08718DB500F5BB4A8BB8DB81999A895D7FA31B5EC56D6743CB2C6821FD`; tests `DA2FA7D152F70372986396C2BC012686F247AF7D9FAA89DF594B551E8C33F4DF`.

## Mandatory next boundaries
1. Authenticate complete current-attempt API snapshots through a trusted collector; never infer the expectation/policy from the evidence being checked.
2. Download the exact artifact IDs, hash actual ZIP bytes against authenticated metadata, and safely decode bounded unique entries. A boolean or supplied observed digest cannot replace this.
3. Revalidate live protected prepublication eligibility through the existing intake contract, including real pending deployments/reviewers and source state, immediately before any authorized integration. This helper adds no lock.
4. Retain human GitHub environment approval and existing required tests; independently verify any publisher dependency/activation on a safe live path. Manual GitHub UI publication remains outside this helper.

The production publisher, final-package workflow, existing eligibility helper, repository settings and Jira completion status were not changed. KAN-133 remains open.
