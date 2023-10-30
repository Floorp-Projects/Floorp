import-content-task-globals
===========================

For files containing ContentTask.spawn calls, this will automatically declare
the frame script variables in the global scope. ContentTask is only available
to test files, so by default the configs only specify it for the mochitest based
configurations.

This saves setting the file as a mozilla/frame-script environment.

Note: due to the way ESLint works, it appears it is only easy to declare these
variables on a file global scope, rather than function global. This may mean that
they are incorrectly allowed, but given they are test files, this should be
detected during testing.
