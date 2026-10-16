# Floorp QA309 independent candidate-01 review

2026-10-09 UTC. Reviewer did not author these changes. Read-only review of tools/qa3/{source.py,test_source.py,README.md}; no source edit, commit, push, workflow dispatch/cancel, browser/native build or publication action.

## Candidate and result

Base/head of worktree: 718e688fbd7ad88a776534e7778ad8e4a7f67364. Uncommitted three-file diff exactly matched floorp-candidate-01.patch: SHA256 ebb5800f550f78c5ddd75e3a5a51e38556b6937df900ee8c6177db11093bd462. source.py SHA256 99698eec43bc357db800a3400969595224f09ed3ffc77f80ec139f8f4a6c2115. These identifiers apply to candidate-01, not a later repaired candidate.

Candidate-01 needs one further cleanup fix (P2, confirmed transient fault path). Parent accepted the observation and is preparing candidate-02. No independent P0/P1 was found in this Floorp review. This does not forbid ordinary existing public hosted PR CI; required promotion and production/native qualification remain outside the synthetic proof.

[Input pin](floorp-independent-input-01.json), [independent script](floorp-independent-negative-01.py), [results](floorp-independent-negative-01.json), [log](floorp-independent-negative-01.log).

## Confirmed P2: a transient cleanup syscall failure aborts all later recovery

Location in the immutable candidate-01 source: _cleanup_owned, source.py lines 385–399, especially os.pidfd_open at 389; _execute_owned cleanup exception handling at 464–473. See candidate-01 patch and source SHA above because later author edits may move lines.

Condition: a single non-ESRCH OSError at cleanup's real pidfd_open call, after an own TERM-ignoring child has started. The bounded fixture injects EMFILE once; every later invocation would succeed. It calls the actual candidate _execute_owned in a dedicated process, not a fake process lifecycle.

Observed: helper returned {exitCode:null,error:"source command timed out"}. The original timeout was retained but secondary cleanup failure was hidden. Own child was still alive after the helper returned because the exception left TERM/KILL/reap loops. The fixture then matched PID birth, sent SIGKILL through its own independent pidfd, waited/reaped, and restored subreaper state.

Evidence: [pidfd-fault-observation.json](floorp-independent-fixtures-01/pidfd-fault-observation.json). One injected syscall fault and one child; no kernel permission change, real resource exhaustion, foreign process killing or giant workload. This proves the transient-exception code path; it is not a claim that ordinary workers routinely experience EMFILE, or that every persistent kernel EPERM can be recovered.

Impact: FAIL remains closed, but an error in cleanup leaves owned work running and erases the cleanup cause from control evidence. That conflicts with the new bounded normal cleanup contract. Suggested repair: collect bounded cleanup errors while continuing independent TERM/KILL/reap steps; keep original error and separate cleanupErrors / ownedCleanupComplete; never accept a result after any cleanup error or cleanup interruption even if later recovery succeeds. Parent has this repair in progress. Candidate-01 evidence stays unchanged.

## Independent execution and successful paths

Executed author tests independently, not a citation of author selftest:
timeout --signal=TERM --kill-after=5s 45s /usr/bin/prlimit --as=536870912 --cpu=20 --fsize=16777216 -- python3 -B -m unittest discover -s tools/qa3 -p test_source.py -v

Exit 0, 21 methods, 5.756s. [Complete suite log](floorp-independent-suite-01.log). These include detached descendant, TERM-late-fork, unrelated pre-existing child, parent subreaper preservation, credential/env restrictions and first-log immutability.

Independent custom fixture ran under 35s wall, 512MiB address space, CPU15s, file16MiB caps; exit 0. Its success means its observations/assertions completed, not that the confirmed cleanup gap passed.

| Category | Observation |
|---|---|
| XML 19 inputs | 17 unsafe/unsupported inputs rejected before ET.fromstring; valid UTF-8 and optional UTF-8 BOM accepted with Unicode identity |
| Encodings | UTF-16/32 BOM and no-BOM LE/BE; UTF-8 bytes declaring UTF-16/32/ISO-8859-1; UTF-8 and BOM DTD/entity documents |
| Real command fd/env | Child inherited only fds 0/1/2; control fd absent; fixture GITHUB_TOKEN, NODE_OPTIONS and PYTHONPATH absent |
| Parent process state | Real execute() did not change caller's subreaper setting |
| Control protocol 9 cases | One typed valid exit accepted; bool/float/string exit, missing fields, JSON null/malformed and reported-error statuses did not return zero |
| PID race models 3 | Birth mismatch before recheck never signalled; ESRCH before pidfd open and before signal safely handled |
| Transient cleanup fault | Confirmed RED described above; fixture cleanup complete and subreaper restored |

Missing-field control currently fails with KeyError, not a normalized ValueError. It still fails closed and the private supervisor's fixed writer does not naturally produce that shape; not counted as a new reachable finding.

UTF-8-only JUnit contract is explicit in README. Decoding before the DTD/entity check and rejecting NUL/other encoding declarations closes the old UTF-16 bypass. Exact case origin/count/completion logic is unchanged. First failure logs are opened exclusively; arbitrary retries were not added. Environment whitelist and publication=false/shadow behavior remain unchanged.

## Complementary evidence and limits

The workflow reviewer separately tested supervisor SIGKILL/SIGSTOP and cleanup-time SIGTERM. Their observation is shared context, not an additional independent run by this reviewer: hard-killed/stopped supervisor causes FAIL but can leave detached children; external VM/job scope teardown is the last-resort boundary. Same-UID isolated Python supervision cannot promise universal containment when its supervisor itself is unconditionally killed. Parent also plans to reject interruption during cleanup in candidate-02.

This reviewer did not execute the full Deno/Node/Python 106-case source plan, real OS-native browser work, private launcher/cgroup provisioning or publication gates. PID reuse was modelled, not forced against unrelated real processes. Kernel pidfd support on this Linux environment was exercised; other OS support is not claimed. The complete post-fix judgement must be made against the new immutable candidate-02 bytes and fresh independent results.

