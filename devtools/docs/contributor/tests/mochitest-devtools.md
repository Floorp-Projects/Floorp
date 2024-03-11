# Automated tests: DevTools mochitests

To run the whole suite of browser mochitests for DevTools (sit back and relax):

```bash
./mach mochitest --subsuite devtools --tag devtools
```
To run a specific tool's suite of browser mochitests:

```bash
./mach mochitest devtools/client/<tool>
```

For example, run all of the debugger browser mochitests:

```bash
./mach mochitest devtools/client/debugger
```
To run a specific DevTools mochitest:

```bash
./mach mochitest devtools/client/path/to/the/test_you_want_to_run.js
```
Note that the mochitests *must* have focus while running. The tests run in the browser which looks like someone is magically testing your code by hand. If the browser loses focus, the tests will stop and fail after some time. (Again, sit back and relax)

In case you'd like to run the mochitests without having to care about focus and be able to touch your computer while running:

```bash
./mach mochitest --headless devtools/client/<tool>
```

You can also run just a single test:

```bash
./mach mochitest --headless devtools/client/path/to/the/test_you_want_to_run.js
```

## Tracing JavaScript

You can log all lines being executed in the mochitest script by using DEBUG_STEP env variable.
This will help you:
 * if the test is stuck on some asynchronous waiting code, on which line it is waiting,
 * visualize the test script execution compared to various logs and assertion logs.

Note that it will only work with Mochitests importing `devtools/client/shared/test/shared-head.js` module,
which is used by most DevTools browser mochitests.

This way:
```bash
DEBUG_STEP=true ./mach mochitest browser_devtools_test.js
```
or that other way:
```bash
./mach mochitest browser_devtools_test.js --setenv DEBUG_STEP=true
```
This will log the following lines:
```
[STEP] browser_target_command_detach.js @ 19:15   ::   const tab = ↦ await addTab(TEST_URL);
```
which tells that test script at line 19 and column 15 is about to be executed.
The '↦' highlights the precise execution's column.

Instead of passing true, you may pass a duration in milliseconds where each test line will pause for a given amount of time.
Be careful when using this feature as it will pause the event loop on each test line and allow another other event to be processed.
This will cause the test to run in a unreal way that wouldn't happen otherwise.

```bash
DEBUG_STEP=250 ./mach mochitest browser_devtools_test.js
```
Each line of the mochitest script will pause for 1/4 of seconds.

Last, but not least, this feature can be used on try via:
```bash
./mach mochitest try fuzzy devtools/test/folder/ --env DEBUG_STEP=true
```
