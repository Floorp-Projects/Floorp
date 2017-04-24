# Automated tests

When working on a patch for DevTools, there's almost never a reason not to add a new test. If you are fixing a bug, you probably should write a new test to prevent this bug from occurring again. If you're implementing a new feature, you should write new tests to cover the aspects of this new feature.

Ask yourself:
* Are there enough tests for my patch?
* Are they the right types of tests?

We use three suites of tests:

* [`xpcshell`](xpcshell.md): Unit-test style of tests. No browser window, only a JavaScript shell. Mostly testing APIs directly.
* [Chrome mochitests](mochitest-chrome.md): Unit-test style of tests, but with a browser window. Mostly testing APIs that interact with the DOM.
* [DevTools mochitests](mochitest-devtools.md): Integration style of tests. Fires up a whole browser window with every test and you can test clicking on buttons, etc.

More information about the different types of tests can be found on the [automated testing page](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Automated_testing) at MDN.

To run all DevTools tests, regardless of suite type:

```bash
./mach test devtools/*
```

Have a look at the child pages for more specific commands for running only a single suite or single test in a suite.
