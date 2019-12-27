# Performance test example: performance of click event in the inspector

Let's look at a trivial but practical example and add a simple test to measure the performance of a click in the inspector.

First we create a file under [tests/inspector](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/tests/inspector) since we are writing an inspector test. We call the file `click.js`.

We will use a dummy test document here: `data:text/html,click test document`.

We prepare the imports needed to write the test, from head.js and inspector-helper.js:
- `testSetup`, `testTeardown`, `openToolbox` and `runTest` from head.js
- `reloadInspectorAndLog` from inspector-helper.js

The full code for the test looks as follows:
```
const {
  reloadInspectorAndLog,
} = require("./inspector-helpers");

const {
  openToolbox,
  runTest,
  testSetup,
  testTeardown,
} = require("../head");

module.exports = async function() {
  // Define here your custom document via a data URI:
  const url = "data:text/html,click test document";

  await testSetup(url);
  const toolbox = await openToolbox("inspector");

  const inspector = toolbox.getPanel("inspector");
  const window = inspector.panelWin; // Get inspector's panel window object
  const body = window.document.body;

  await new Promise(resolve => {
    const test = runTest("inspector.click");
    body.addEventListener("click", function () {
      test.done();
      resolve();
    }, { once: true });
    body.click();
  });

  // Check if the inspector reload is impacted by click
  await reloadInspectorAndLog("click", toolbox);

  await testTeardown();
}
```

Finally we add an entry in [damp-tests.js](https://searchfox.org/mozilla-central/source/testing/talos/talos/tests/devtools/addon/content/damp-tests.js):
```
  {
    name: "inspector.click",
    path: "inspector/click.js",
    description:
      "Measure the time to click in the inspector, and reload the inspector",
  },
```

Then we can run our test with:
```
./mach talos-test --activeTests damp --subtest inspector.click
```

