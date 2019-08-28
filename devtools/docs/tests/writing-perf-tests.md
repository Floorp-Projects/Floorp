# Writing new performance test

See [Performance tests (DAMP)](performance-tests.md) for an overall description of our performance tests.
Here, we will describe how to write a new test and register it to run in DAMP.

{% hint style="tip" %}

**Reuse existing tests if possible!**

If a `custom` page already exists for the tool you are testing, try to modify the existing `custom` test rather than adding a new individual test.

New individual tests run separately, in new tabs, and make DAMP slower than just modifying existing tests. Complexifying `custom` test pages should also help cover more scenarios and catch more regressions. For those reasons, modifying existing tests should be the preferred way of extending DAMP coverage.

`custom` tests are using complex documents that should stress a particular tool in various ways. They are all named `custom.${tool}` (for instance `custom.inspector`). The test pages for those tests can be found in [pages/custom](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/pages/custom).

If your test case requires a dedicated document or can't run next to the other tests in the current `custom` test, follow the instructions below to add a new individual test.

{% endhint %}

This page contains the general documentation for writing DAMP tests. See also:
- [Performance test writing example](./writing-perf-tests-example.html) for a practical example of creating a new test
- [Performance test writing tips](./writing-perf-tests-tips.html) for detailed tips on how to write a good and efficient test

## Test location

Tests are located in [testing/talos/talos/tests/devtools/addon/content/tests](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests). You will find subfolders for panels already tested in DAMP (debugger, inspector, …) as well as other subfolders for tests not specific to a given panel (server, toolbox).

Tests are isolated in dedicated files. Some examples of tests:
- [tests/netmonitor/simple.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/netmonitor/simple.js)
- [tests/inspector/mutations.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/inspector/mutations.js)

## Basic test

The basic skeleton of a test is:

```
const {
  testSetup,
  testTeardown,
  SIMPLE_URL,
} = require("../head");

module.exports = async function() {
  await testSetup(SIMPLE_URL);

  // Run some measures here

  await testTeardown();
};
```

* always start the test by calling `testSetup(url)`, with the `url` of the document to use
* always end the test with `testTeardown()`


## Test documents

DevTools performance heavily depends on the document against which DevTools are opened. There are two "historical" documents you can use for tests for any panel:
* "Simple", an empty webpage. This one helps highlighting the load time of panels,
* "Complicated", a copy of bild.be, a German newspaper website. This allows us to examine the performance of the tools when inspecting complicated, big websites.

The URL of those documents are exposed by [tests/head.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/head.js). The Simple page can be found at [testing/talos/talos/tests/devtools/addon/content/pages/simple.html](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/pages/simple.html). The Complicated page is downloaded via [tooltool](https://wiki.mozilla.org/ReleaseEngineering/Applications/Tooltool) automatically the first time you run the DAMP tests.

You can create also new test documents under [testing/talos/talos/tests/devtools/addon/content/pages](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/pages). See the pages in the `custom` subfolder for instance. If you create a document in `pages/custom/mypanel/index.html`, the URL of the document in your tests should be `PAGES_BASE_URL + "custom/mypanel/index.html"`. The constant `PAGES_BASE_URL` is exposed by head.js.

Note that modifying any existing test document will most likely impact the baseline for existing tests.

Finally you can also create very simple test documents using data urls. Test documents don't have to contain any specific markup or script to be valid DAMP test documents, so something as simple as `testSetup("data:text/html,my test document");` is valid.


## Test helpers

Helper methods have been extracted in shared modules:
* [tests/head.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/head.js) for the most common ones
* tests/{subfolder}/{subfolder}-helpers.js for folder-specific helpers ([example](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/inspector/inspector-helpers.js))

To measure something which is not covered by an existing helper, you should use `runTest`, exposed by head.js.

```
module.exports = async function() {
  await testSetup(SIMPLE_URL);

  // Calling `runTest` will immediately start recording your action duration.
  // You can execute any necessary setup action you don't want to record before calling it.
  const test = runTest(`mypanel.mytest.mymeasure`);

  await doSomeThings(); // <== Do an action you want to record here

  // Once your action is completed, call `runTest` returned object's `done` method.
  // It will automatically record the action duration and appear in PerfHerder as a new subtest.
  // It also creates markers in the profiler so that you can better inspect this action in
  // profiler.firefox.com.
  test.done();

  await testTeardown();
};
```

If your measure is not simply the time spent by an asynchronous call (for instance computing an average, counting things…) there is a lower level helper called `logTestResult` which will directly log a value. See [this example](https://searchfox.org/mozilla-central/rev/325c1a707819602feff736f129cb36055ba6d94f/testing/talos/talos/tests/devtools/addon/content/tests/webconsole/streamlog.js#62).


## Test runner

If you need to dive into the internals of the DAMP runner, most of the logic is in [testing/talos/talos/tests/devtools/addon/content/damp.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/damp.js).


# How to name your test and register it?

If a new test file was created, it needs to be registered in the test suite. To register the new test, add it in [damp-tests.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/damp-tests.js). This file acts as the manifest for the DAMP test suite.

If your are writing a test executing against Simple and Complicated documents, your test name will look like: `(simple|complicated).${tool-name}.${test-name}`.
So for our example, it would be `simple.inspector.click` and `complicated.inspector.click`.
For independent tests that don't use the Simple or Complicated documents, the test name only needs to start with the tool name, if the test is specific to that tool
For the example, it would be `inspector.click`.

In general, the test name should try to match the path of the test file. As you can see in damp-tests.js this naming convention is not consistently followed. We have discrepencies for simple/complicated/custom tests, as well as for webconsole tests. This is largely for historical reasons.


# How to run your new test?

You can run any performance test with this command:
```
./mach talos-test --activeTests damp --subtest ${your-test-name}
```

By default, it will run the test 25 times. In order to run it just once, do:
```
./mach talos-test --activeTests damp --subtest ${your-test-name} --cycles 1 --tppagecycles 1
```
`--cycles` controls the number of times Firefox is restarted
`--tppagecycles` defines the number of times we repeat the test after each Firefox start

Also, you can record a profile while running the test. To do that, execute:
```
./mach talos-test --activeTests damp --subtest ${your-test-name} --cycles 1 --tppagecycles 1 --geckoProfile --geckoProfileEntries 100000000
```
`--geckoProfiler` enables the profiler
`--geckoProfileEntries` defines the profiler buffer size, which needs to be large while recording performance tests

Once it is done executing, the profile lives in a zip file you have to uncompress like this:
```
unzip testing/mozharness/build/blobber_upload_dir/profile_damp.zip
```
Then you have to open [https://profiler.firefox.com/](https://profiler.firefox.com/) and manually load the profile file that lives here: `profile_damp/page_0_pagecycle_1/cycle_0.profile`

