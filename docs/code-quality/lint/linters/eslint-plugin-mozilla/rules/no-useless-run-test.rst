no-useless-run-test
===================

Designed for xpcshell-tests. Rejects definitions of ``run_test()`` where the
function only contains a single call to ``run_next_test()``. xpcshell's head.js
already defines a utility function so there is no need for duplication.
