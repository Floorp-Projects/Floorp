dump("hello from the debugee!\n");
// We should hit the above dump as we set a breakpoint on the first line

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This is a file that test_xpcshell_debugging.js debugs.

debugger; // and why not check we hit this!?

dump("try to set a breakpoint here");
