# Test for special asmjs SIGSEGV-handling.
#
# Expected behavior is for the asm.js code in the following fragment to trigger
# SIGSEGV. The code in js/src/gdb/mozilla/asmjs.py should prevent GDB from
# handling that signal.
# flake8: noqa: F821

run_fragment('asmjs.segfault')

# If SIGSEGV handling is broken, GDB would have stopped at the SIGSEGV signal.
# The breakpoint would not have hit, and run_fragment would have thrown.
#
# So if we get here, and the asm.js code actually ran, we win.

assert_pretty('ok', 'true')
assert_pretty('rval', '$JS::Value("ok")')
