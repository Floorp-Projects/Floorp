# Allocation tests

This folder contains special mochitests which are meant to record data about the memory usage of DevTools.
This uses Spidermonkey's Memory API implemented next to the debugger API.
For more info, see the following doc:
https://searchfox.org/mozilla-central/source/js/src/doc/Debugger/Debugger.Memory.md

# Test example

```
add_task(async function() {
  // Execute preliminary setup in order to be able to run your scenario
  // You would typicaly load modules, open a tab, a toolbox, ...
  ...

  // Run the test scenario first before recording in order to load all the
  // modules. Otherwise they get reported as "still allocated" objects,
  // whereas we do expect them to be kept in memory as they are loaded via
  // the main DevTools loader, which keeps the module loaded until the
  // shutdown of Firefox
  await testScript();

  // Pass alsoRecordContentProcess if you want to record the content process
  // of the current tab. Otherwise it will only record parent process objects.
  const recordData = await startRecordingAllocations({ alsoRecordContentProcess: true });

  // Now, run the test script. This time, we record this run.
  await testScript(toolbox);

  // This will stop the record and also publish the results to Talos database
  // Second argument will be the name of the test displayed in Talos.
  // Many tests will be recorded, but all of them will be prefixed with this string.
  await stopRecordingAllocations(recordData, "reload");

  // Then, here you can execute cleanup.
  // You would typically close the tab, toolbox, ...
});
```

# How to run them locally

```
$ ./mach mochitest --headless devtools/client/framework/test/allocations/
```

And to only see the results:
```
$ ./mach mochitest --headless devtools/client/framework/test/allocations/ | grep " test leaked "
```

# How to easily get data from try run

```
$ ./mach try fuzzy devtools/client/framework/test/allocations/ --query "'linux 'chrome-e10s 'opt '64-qr/opt"
```
You might also pass `--rebuild 3` if the test result is having some noise and you want more test runs.

# Following trends for these tests

You may try looking at:
https://firefox-dev.tools/performance-dashboard/tools/memory.html

Or at:
https://treeherder.mozilla.org/perfherder/graphs?highlightAlerts=1&highlightChangelogData=1&series=mozilla-central,3765893,1,12&timerange=31536000
Link that you get from: https://treeherder.mozilla.org/perfherder/graphs
by looking at last year data for "DevTools" in the first dropdown,
and double clicking on the relevant line in "Tests" menulist.

Significant improvements and regressions will be notified through the following dashboard:
https://treeherder.mozilla.org/perfherder/alerts?hideDwnToInv=1&page=1&framework=12
