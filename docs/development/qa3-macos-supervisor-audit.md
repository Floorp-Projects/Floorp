# QA3 macOS supervisor audit (KAN-130)

D-17 uses GitHub Actions as the runner. The
[supervisor workflow](../../.github/workflows/qa3-macos-supervisor.yml) runs on
GitHub-hosted Apple silicon (`macos-15`) and Intel (`macos-15-intel`) for
relevant pull requests and manual dispatch. Each job uses the runner's `python3`
and `/usr/bin/python3`.
[Run 44889172049](https://github.com/Floorp-Projects/Floorp/actions/runs/44889172049)
ran all 57 tests on each image with both interpreters.

## 2-E coverage

- **Reap owned descendants.** A native libproc test verifies that an orphan is
  found by session membership after reparenting and exits during cleanup.
  Another verifies that a descendant tracked before calling `setsid` exits.
- **Spare unrelated processes.** A native test starts a same-user process in
  another session after spawning the supervised command and verifies that it is
  still alive after cleanup. Simulated tests exercise PID and session ID reuse
  and denied inspection.
- **Fail when cleanup is incomplete.** A native fault-injection test keeps an
  owned descendant alive after the command exits successfully and requires a
  cleanup error and a QA `FAIL` verdict. Simulated tests check that an
  unverifiable owned process or session also fails cleanup.

The workflow now fails on skipped tests or empty discovery and attempts the
suite with both interpreters even if the first fails. The native fault test also
asserts that the command exited with code zero and reported no command error.

macOS session tracking has limits. A descendant that leaves the session and
becomes orphaned before it is first observed can escape ownership. The scan is
UID-scoped, so an untracked descendant that changes UID can also escape. Tracked
PIDs are checked again before signalling, with a residual race between the
identity read and the signal. Hard runner termination can interrupt cleanup. See
the [supervisor design](../../tools/qa3/README.md) and
[contract tests](../../tools/qa3/test_source_macos.py) for ownership rules and
the fail-closed behavior for known survivors or uncertain inspections. The
hosted jobs cover the 2-E supervisor. The QA3 source-plan executor remains
Linux-only.
