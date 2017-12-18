# Writing new performance test

See [Performance tests (DAMP)](performance-tests.md) for an overall description of our performance tests.
Here, we will describe how to write a new test with an example: track the performance of clicking inside the inspector panel.

## Where is the code for the test?

For now, all the tests live in a single file [damp.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/damp.js).

There are two kinds of tests:
* The first kind is being run against two documents:
  * "Simple", an empty webpage. This one helps highlighting the load time of panels,
  * "Complicated", a copy of bild.be, a German newspaper website. This allows us to examine the performance of the tools when inspecting complicated, big websites.

  To run your test against these two documents, add it to [this function](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.js#563-673).

  Look for `_getToolLoadingTests` function. There is one method per tool. Since we want to test how long does it take to click on the inspector, we will find the `inspector` method, and add the new test code there, like this:
  ```
  _getToolLoadingTests(url, label, { expectedMessages, expectedSources }) {
    let tests = {
      async inspector() {
        await this.testSetup(url);
        let toolbox = await this.openToolboxAndLog(label + ".inspector", "inspector");
        await this.reloadInspectorAndLog(label, toolbox);

        // <== here we are going to add some code to test "click" performance,
        //     after the inspector is opened and after the page is reloaded.

        await this.closeToolboxAndLog(label + ".inspector");
        await this.testTeardown();
      },
  ```
* The second kind isn't specific to any document. You can come up with your own test document, or not involve any document if you don't need one.
  If that better fits your needs, you should introduce an independent test function. Like [this one](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.js#330-348) or [this other one](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.js#350-402). You also have to register the new test function you just introduced in this [`tests` object within `startTest` function](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.js#863-864).

  You could also use extremely simple documents specific to your test case like this:
  ```
  /**
   * Measure the time necessary to click in the inspector panel
   */
  async _inspectorClickTest() {
    // Define here your custom document via a data URI:
    let url = "data:text/html,custom test document";
    let tab = await this.testSetup(url);
    let messageManager = tab.linkedBrowser.messageManager;
    let toolbox = await this.openToolbox("inspector");

    // <= Here, you would write your test actions,
    // after opening the inspector against a custom document

    await this.closeToolbox();
    await this.testTeardown();
  },
  ...
  startTest(doneCallback, config) {
    ...
    // And you have to register the test in `startTest` function
    // `tests` object is keyed by test names. So our test is named "inspector.click" here.
    tests["inspector.click"] = this._inspectorClickTest;
    ...
  }

  ```

## How to name your test and register it?

If your are writing a test executing against Simple and Complicated documents, your test name will look like: `(simple|complicated).${tool-name}.${test-name}`.
So for our example, it would be `simple.inspector.click` and `complicated.inspector.click`.
For independent tests that don't use the Simple or Complicated documents, the test name only needs to start with the tool name, if the test is specific to that tool
For the example, it would be `inspector.click`.

Once you come up with a name, you will have to register your test [here](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.html#12-42) and [here](https://searchfox.org/mozilla-central/rev/cd742d763809089925a38178dd2ba5a9069fa855/testing/talos/talos/tests/devtools/addon/content/damp.html#44-71) with a short description of it.

## How to write a performance test?

When you write a performance test, in most cases, you only care about the time it takes to complete a very precise action.
There is a `runTest` helper method that helps recording a precise action duration:
```
// Calling `runTest` will immediately start recording your action duration.
// You can execute any necessary setup action you don't want to record before calling it.
let test = this.runTest("my.test.name"); // `runTest` expects the test name as argument

// <== Do an action you want to record here

// Once your action is completed, call `runTest` returned object's `done` method.
// It will automatically record the action duration and appear in PerfHerder as a new subtest.
// It also creates markers in the profiler so that you can better inspect this action in perf-html.
test.done();
```

So for our click example it would be:
```
async inspector() {
  await this.testSetup(url);
  let toolbox = await this.openToolboxAndLog(label + ".inspector", "inspector");
  await this.reloadInspectorAndLog(label, toolbox);

  let inspector = toolbox.getPanel("inspector");
  let window = inspector.panelWin; // Get inspector's panel window object
  let body = window.document.body;

  await new Promise(resolve => {
    let test = this.runTest("inspector.click");
    body.addEventListener("click", function () {
      test.done();
      resolve();
    }, { once: true });
    body.click();
  });
}
```

## How to run your new test?

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
Then you have to open [https://perf-html.io/](https://perf-html.io/) and manually load the profile file that lives here: `profile_damp/page_0_pagecycle_1/cycle_0.profile`

## How to write a good performance test?

### Verify that you wait for all asynchronous code

If your test involves asynchronous code, which is very likely given the DevTools codebase, please review carefully your test script.
You should ensure that _any_ code ran directly or indirectly by your test is completed.
You should not only wait for the functions related to the very precise feature you are trying to measure.

This is to prevent introducing noise in the test run after yours. If any asynchronous code is pending,
it is likely to run in parallel with the next test and increase its variance.
Noise in the tests makes it hard to detect small regressions.

You should typically wait for:
* All RDP requests to finish,
* All DOM Events to fire,
* Redux action to be dispatched,
* React updates,
* ...

### Ensure that its results change when regressing/fixing the code or feature you want to watch.

If you are writing the new test to cover a recent regression and you have a patch to fix it, push your test to try without _and_ with the regression fix.
Look at the try push and confirm that your fix actually reduces the duration of your perf test significantly.
If you are introducing a test without any patch to improve the performance, try slowing down the code you are trying to cover with a fake slowness like `setTimeout` for asynchronous code, or very slow `for` loop for synchronous code. This is to ensure your test would catch a significant regression.

For our click performance test, we could do this from the inspector codebase:
```
window.addEventListener("click", function () {

  // This for loop will fake a hang and should slow down the duration of our test
  for (let i = 0; i < 100000000; i++) {}

}, true); // pass `true` in order to execute before the test click listener
```

### Keep your test execution short.

Running performance tests is expensive. We are currently running them 25 times for each changeset landed in Firefox.
Aim to run tests in less than a second on try.
