This directory contains tests that are flaky when run with other tests
but that we don't want to disable and where it's not trivial to make
the tests not flaky at this time, but we have a plan to fix them via
systemic fixes that are improving the codebase rather than hacking a
test until it works.

This directory and ugly hack structure needs to exist because of
multi-e10s propagation races that will go away when we finish
implementing the multi-e10s overhaul for ServiceWorkers.  Most
specifically, unregister() calls need to propagate across all
content processes.  There are fixes on bug 1318142, but they're
ugly and complicate things.

Specific test notes and rationalizations:
- multi-e10s-update: This test relies on there being no registrations
  existing at its start.  The preceding test that induces the breakage
  (`browser_force_refresh.js`) was made to clean itself up, but the
  unregister() race issue is not easily/cleanly hacked around and this
  test will itself become moot when the multi-e10s changes land.
