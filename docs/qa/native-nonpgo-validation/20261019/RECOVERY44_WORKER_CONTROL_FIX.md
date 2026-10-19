# Recovery44: separate finite worker controls from timed import qualification

Recovery43 run 39194618542 failed the tiny real-child unit before the actual helper-import comparison. The exact inner cause was not retained and remains unknown. Its 0.25-second workload combined independent child deadlines with coordinator liveness admission; scheduling delay is a plausible counterexample, not a demonstrated cause of that run. Spawn, readiness, filesystem and cleanup failures remain possible.

The real-child unit now requests four fixed iterations with 1 MiB per worker and 1 MiB disk work for one worker. It verifies the ready/start barrier, inherited process group, actual memory/hash/disk/fsync counters, natural exit, exact-child reap, descriptor closure and bound disk removal. This control makes no duration qualification claim. The duration reader rejects a finite receipt, and production timed worker arguments retain duration mode.

The separate Mac diagnostic still performs fresh helper queries at the unchanged three-second deadline under idle and real 60-second bounded contention. Its two 128 MiB workers, 64 MiB disk work, six pairs per phase, live-worker predicates, resource bounds, first-failure handling and exact-child cleanup remain required. The support-preimport variant is a heavy-import control, not an exact replay of the old program or an OS cold-cache measurement.

One fixed-case structured marker preserves safe control state on success and failure. It retains the primary phase/code and independent cleanup failure, numeric counters and disk-removal state. Projection drops process identities, paths, arguments, free exception text and unknown keys. The unittest receipt requires the exact verbose case prefix, one valid marker and a passing control for a passing suite; missing, duplicated, malformed or mismatched evidence cannot qualify.

The production helper import isolation from recovery43 is retained. Production resource limits, three-second helper deadline, bounded retries, ownership/birth checks, unknown handling and cleanup policy are unchanged. SDK policy, storage/retention, native harness and product sources are unchanged by this follow-up.

Short44 must pass the exact bound QA and recording suites without skips, run the actual idle/contended comparison, and receive a terminal independent review before long45 activation. These short results cannot establish full host/XUL or native Shim authentication, and do not establish the original helper-timeout or native-exit cause.
