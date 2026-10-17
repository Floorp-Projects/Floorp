# Runtime CI diagnostics patch

`mac-shim-failure-diagnostics.patch` applies to Runtime
`7dd619742b25567e479eeda97ae071d10542ad0b`. It changes three workflow files;
it does not change native code, authentication checks, timeouts or publish gates.
This directory is separate from `common/`: the package workflow does not apply
this CI patch. It must be reviewed and applied to a Runtime source branch.

The verifier pins Floorp fixture commit
`73c6d1e5326999adfceac2c2184e0b964938fcd4`. That commit must be available
remotely before the patched workflow can use it. Record the actual fixture,
workflow and producer commits separately in each diagnostic run.

The context-menu probe retains `first-failure.json` before querying diagnostics
or cleaning up. `before-cleanup.json` and `result.json` add the last phase,
attempted iteration count, bounded authentication events and process observations.
Cleanup failures remain separate from the original failure. An unauthenticated
Shim has no observed kernel exit status; it is reported as unknown. The browser
exit status is kept separate from the LaunchServices helper's status, and cleanup
signals remain attributable to cleanup.

Only the verified isolated browser receives diagnostic or cleanup commands.
Authentication predicates, native process ownership checks, context-menu
assertions, iteration requirements and wait durations are retained.

The probe captures raw launcher/browser streams under `.private-logs/` inside
its private temporary directory. The uploaded `browser.log`, `browser.stdout.log`
and `browser.stderr.log` contain JSON summaries with fixed error markers/codes
and byte/truncation metadata. Arbitrary text, cookies, tokens, nonces, command
lines and error response bodies are omitted. Each stream reads at most a 64 KiB
tail; marker and code lists each retain at most 64 entries. The new authentication
event ring retains at most 64 events and excludes arbitrary payloads. The upload
uses explicit filenames and `include-hidden-files: false`; private raw files are
never selected. Unknown messages are omitted, so these diagnostics cannot promise
to identify every root cause. Existing functional event arrays and raw on-disk
capture do not gain a total execution memory/disk limit from this change.

The daily caller forwards the same requested `debug` expression used by builders
to all five native verifiers. Linux Debug verifiers additionally run their
existing conditional rendering check; the profile-path check remains mandatory.
Darwin displays it as `REQUESTED_DEBUG_BUILD`, with
an explicit note that compiled build mode is unverified. The generic native
startup/rendering check does not establish `MOZ_DEBUG`. Debug qualification still
requires build configuration evidence. Production-opt and PGO generation are
unchanged.

Run the portable regression tests without a browser or network:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tools/app-shim -p test_context_menu_diagnostics.py -v
```

After separately authorizing CI, the existing verifier can consume the original
universal artifact in a new diagnostic run: source run `38963901545`, artifact
`12082552649`, BuildID `20261016204014`, Darwin/x86_64, `macos-15-intel`, requested
Debug `true`. Revalidate provider metadata and the published workflow/fixture
refs first. Use a new run so original failed attempt 1 and artifact `12082840607`
remain intact. A passing diagnostic does not replace that failure or qualify a
signed distribution. This route needs no Runtime rebuild and no Blacksmith runner.

The companion push caller is restricted to Runtime branch
`codex/mac-shim-native-diagnostic-20261017`, with an additional exact repository,
event and ref condition. It calls only the native verifier on `macos-15-intel`,
using the fixed original run/artifact/BuildID above and requested Debug `true`.
The verifier retains its 30-minute timeout. No secrets are inherited, and token
permissions are limited to reading Actions and repository contents. Existing
Runtime build push filters do not match this branch. A push to this branch is
an execution request; do not repush to retry a failed diagnostic. Record any
new run separately from original attempt 1. The caller has no schedule, manual
dispatch, matrix, build, signing credential, publication or approval job.

The follow-up fixture adds a passive, owned-instance Shim exit watcher and
scoped OS crash/dependency summaries for the same original artifact. It records
actual returned kernel wait status and observation times; missing observations
remain unknown. No connected notification does not prove native authentication
never completed. Raw argv/tokens, raw crash reports and private streams are not
uploaded. The separately maintained native cause-preservation patch in
../native-diagnostics/ requires recompilation and is not applied by this job.
Each new push requires a distinct recorded diagnostic purpose; neither previous
failed run is rerun. Authentication, waits, iterations and release gates remain.

The next purpose-specific diagnostic also collects fixed OS-log marker labels for
the verified host/Shim emitter PIDs and exact image paths, within launch-to-cleanup
time. It queries at most the last two minutes with a ten-second command timeout,
without privilege escalation. Published summaries read at most 1 MiB and retain
64 matching events with explicit counts/truncation; raw temporary spool size is
not given a hard total-disk bound. Raw messages and paths are not uploaded.
Other daemon emitters and older history are outside this scope. Unavailable or
empty results cannot exclude an OS/authentication cause, and markers alone do
not establish causality. Record this as one additional attempt with new evidence,
not as replacement of the two previous failed runs or the healthy observed run.
