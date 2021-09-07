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
  await startRecordingAllocations({ alsoRecordContentProcess: true });

  // Now, run the test script. This time, we record this run.
  await testScript(toolbox);

  // This will stop the record and also publish the results to Talos database
  // Second argument will be the name of the test displayed in Talos.
  // Many tests will be recorded, but all of them will be prefixed with this string.
  await stopRecordingAllocations("reload", { alsoRecordContentProcess: true });

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

# Debug leaks

If you are seeing a regression or an improvement, only seeing the number of objects being leaked isn't super helpful.
The tests includes some special debug modes which are printing lots of data to figure out what is leaking and why.

You may run the test with the following env variable to turn debug mode on:
```
DEBUG_DEVTOOLS_ALLOCATIONS=leak|allocations $ ./mach mochitest --headless devtools/client/framework/test/allocations/the-fault-test.js
```

DEBUG_DEVTOOLS_ALLOCATIONS can enable two distinct debug output. (Only one can be enabled at a given time)

DEBUG_DEVTOOLS_ALLOCATIONS=allocations will report all allocation sites that have been made
while running your test. This will include allocations which has been freed.
This view is especially useful if you want to reduce allocations in order to reduce GC overload.

DEBUG_DEVTOOLS_ALLOCATIONS=leak will report only the allocations which are still allocated
at the end of your test. Sometimes it will only report allocations with missing stack trace.
Thus making the preview view helpful.

## Example

Let's assume we have the following code:
```
 1:  exports.MyModule = {
 2:    globalArray: [],
 3:    test() {
 3:      // The following object will be allocated but not leaked,
 5:      // as we keep no reference to it anywhere
 6:      const transientObject = {};
 7:
 8:      // The following object will be allocated on this line,
 9:      // but leaked on the following one. By storing a reference
10:      // to it in the global array which is never cleared.
11:      const leakedObject = {};
12:      this.globalArray.push(leakedObject);
13:    },
14:  };

And that, we have a memory test doing this:
```
  const { MyModule } = require("devtools/my-module");

  await startRecordingAllocations();

  MyModule.test();

  await stopRecordingAllocations("target");
```

We can first review all the allocations by running:
```
DEBUG_DEVTOOLS_ALLOCATIONS=allocations $ ./mach mochitest --headless devtools/client/framework/test/allocations/browser_allocation_myTest.js

```
which will print at the end:
```
DEVTOOLS ALLOCATION: all allocations (which may be freed or are still allocated):
[
   {
     "src": "UNKNOWN",
     "count": 80,
     "lines": [
       "?: 80"
     ]
   },
   {
     "src": "resource://devtools/my-module.js",
     "count": 2,
     "lines": [
       "11: 1"
       "6: 1"
     ]
   }
]
```
The first part, with "UNKNOWN" can be ignored. This is about objects with missing allocation sites.
The second part of this logs tells us that 2 objects were allocated from my-module.js when running the test.
One has been allocated at line 6, it is `transcientObject`.
Another one has been allocated at line 11, it is `leakedObject`.

Now, we can use the second view to focus only on objects that have been kept allocated:
```
DEBUG_DEVTOOLS_ALLOCATIONS=leaks $ ./mach mochitest --headless devtools/client/framework/test/allocations/browser_allocation_myTest.js

```
which will print at the end:
```
DEVTOOLS ALLOCATION: allocations which leaked:
[
   {
     "src": "UNKNOWN",
     "count": 80,
     "lines": [
       "?: 80"
     ]
   },
   {
     "src": "resource://devtools/shared/commands/commands-factory.js",
     "count": 1,
     "lines": [
       "11: 1"
     ]
   }
]
```
Similarly, we can focus only on the second part, which tells us that only one object is being leaked
and this object has been originally created from line 11, this is `leakedObject`.
This doesn't tell us why the object is being kept allocated, but at least we know which one is being kept in memory.

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
