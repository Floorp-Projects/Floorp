If you're running these tests and you get an error like this:

FAIL head.js import threw an exception - Error opening input stream (invalid filename?): chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js

Then run `mach test toolkit/mozapps/update/tests/browser` first.  You can
stop mach as soon as it starts the first test, but this is necessary so that
mach builds the update tests in your objdir.
