# KAN-24 dependent and current-main standard CI

Both standard Ubuntu browser suites executed the submitted observer and settings regression modules successfully. Their `runAllTests` functions declare seven observer cases and three real Services/RPC settings cases. Artifact logs establish successful module completion, not individually logged successful case results.

| Subject | Source | Runtime | Standard browser result | Artifact |
| --- | --- | --- | --- | --- |
| Dependent Draft3046 | d058d7001b822f0ef2413b1307c98a463dfdcdb9, fixed canonical2950 base69d |158.0.1 /20261014104253 |267modules passed,0failed; downloaded Firefox8files/16tasks passed |13445949715 |
| Current-main integration |493d2974e00e6902d86585d329eea1f9040ff791, main29a3+canonical2950+supplement |159.0.1 /20261103153232 |284modules passed,0failed; downloaded Firefox8files/16tasks passed |13446747884 |

Run42093984715 completed success across all jobs. Run42094068516 Ubuntu/smoke/Windows/ARM succeeded. Attempt1 macOS native-validation job was cancelled before acquiring any runner (runner_id0, no steps), with GitHub annotations explicitly reporting hosted macOS arm64 capacity constraints. This infrastructure failure did not execute product code. Only that macOS job was retried once; attempt2 acquired runner1000052238 and completed success. The integration workflow is now success; the prior capacity failure is retained as infrastructure evidence. Successful Ubuntu browser execution was not repeated. Original attempt1 metadata/annotations remain separate. That native validation job is not original macOS Photon UI reproduction.

Both artifact ZIP SHA256 values match GitHub API artifact digests. See `*-artifact-verification.json`, `target-test-execution.json`, metadata and extracted logs. Existing-source158 and current-main159 are distinct tests. Standard runner validation is not full production package validation.

The shell GitHub client could not follow the temporary artifact redirect (HTTP403); the connected GitHub artifact tool successfully retrieved the same artifacts. This was an evidence-download transport failure, not a product or test failure. No signed URLs were saved in evidence. No test/protection settings were disabled to obtain success.

Public check-run evidence uses `integration-macos-check-public.json` and `attempt1-integration-macos-check-public.json`, retaining status/output but removing application-owner account metadata. Original raw records remain local and are excluded from shared QA.
