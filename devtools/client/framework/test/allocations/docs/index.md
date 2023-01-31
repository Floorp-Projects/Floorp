# Allocation tests

The [allocations](https://searchfox.org/mozilla-central/source/devtools/client/framework/test/allocations) folder contains special mochitests which are meant to record data about the memory usage of DevTools.
This uses Spidermonkey's Memory API implemented next to the debugger API.
For more info, see the following doc:
<https://searchfox.org/mozilla-central/source/js/src/doc/Debugger/Debugger.Memory.md>

# Test example

```javascript
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

```bash
$ ./mach mochitest --headless devtools/client/framework/test/allocations/
```

And to only see the results:
```bash
$ ./mach mochitest --headless devtools/client/framework/test/allocations/ | grep " test leaked "
```

# Debug leaks

If you are seeing a regression or an improvement, only seeing the number of objects being leaked isn't super helpful.
The tests includes some special debug modes which are printing lots of data to figure out what is leaking and why.

You may run the test with the following env variable to turn debug mode on:
```bash
DEBUG_DEVTOOLS_ALLOCATIONS=leak|allocations $ ./mach mochitest --headless devtools/client/framework/test/allocations/the-fault-test.js
```

**DEBUG_DEVTOOLS_ALLOCATIONS** can enable two distinct debug output. (Only one can be enabled at a given time)

**DEBUG_DEVTOOLS_ALLOCATIONS=allocations** will report all allocation sites that have been made
while running your test. This will include allocations which has been freed.
This view is especially useful if you want to reduce allocations in order to reduce GC overload.

**DEBUG_DEVTOOLS_ALLOCATIONS=leak** will report only the allocations which are still allocated
at the end of your test. Sometimes it will only report allocations with missing stack trace.
Thus making the preview view helpful.

## Example

Let's assume we have the following code:

```javascript
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
```

And that, we have a memory test doing this:

```javascript
  const { MyModule } = require("devtools/my-module");

  await startRecordingAllocations();

  MyModule.test();

  await stopRecordingAllocations("target");
```

We can first review all the allocations by running:

```bash
DEBUG_DEVTOOLS_ALLOCATIONS=allocations $ ./mach mochitest --headless devtools/client/framework/test/allocations/browser_allocation_myTest.js

```

which will print at the end:

```javascript
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

The first part, with `UNKNOWN` can be ignored. This is about objects with missing allocation sites.
The second part of this logs tells us that 2 objects were allocated from my-module.js when running the test.
One has been allocated at line 6, it is `transcientObject`.
Another one has been allocated at line 11, it is `leakedObject`.

Now, we can use the second view to focus only on objects that have been kept allocated:

```bash
DEBUG_DEVTOOLS_ALLOCATIONS=leaks $ ./mach mochitest --headless devtools/client/framework/test/allocations/browser_allocation_myTest.js

```

which will print at the end:

```javascript
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


## Debug leaks via dominators

This last feature might be the most powerful and isn't bound to DEBUG_DEVTOOLS_ALLOCATIONS.
This is always enabled.
Also, it requires to know which particular object is being leaked and also require to hack
the codebase in order to pass a reference of the suspicious object to the test helper.

You can instruct the test helper to track a given object by doing this:

```javascript
 1: // Let's say it is some code running from "my-module.js"
 2:
 3: // From a DevTools CommonJS module:
 4: const { track } = require("devtools/shared/test-helpers/tracked-objects.sys.mjs");
 5: // From anything else, JSM, XPCOM module,...:
 6: const { track } = ChromeUtils.importESModule("resource://devtools/shared/test-helpers/tracked-objects.sys.mjs");
 7:
 8: const g = [];
 9: function someFunctionInDevToolsCalledBySomething() {
10:   const myLeakedObject = {};
11:   track(myLeakedObject);
12:
13:   // Simulate a leak by holding a reference to the object in a global `g` array
14:   g.push({ seeMyCustomAttributeHere: myLeakedObject });
15: }
```

Then, when running the test you will get such output:

```bash
 0:41.26 GECKO(644653)  # Tracing: Object@my-module:10
 0:40.65 GECKO(644653) ### Path(s) from root:
 0:41.26 GECKO(644653) - other@no-stack:undefined.WeakMap entry value
 0:41.26 GECKO(644653)  \--> LexicalEnvironment@base-loader.sys.mjs:160.**UNKNOWN SLOT 1**
 0:41.26 GECKO(644653)  \--> Object@base-loader.sys.mjs:155.g
 0:41.26 GECKO(644653)  \--> Array@my-module.js:8.objectElements[0]
 0:41.26 GECKO(644653)  \--> Object@my-module.js:14.seeMyCustomAttributeHere
 0:41.26 GECKO(644653)  \--> Object@my-module.js:10
```

This output means that `myLeakedObject` was originally allocated from my-module.js at line 10.
And is being held allocated because it is kept in an Object allocated from my-module.js at line 14.
This is our custom object we stored in `g` global Array.
This custom object it hold by the Array allocated at line 8 of my-module.js.
And this array is held allocated from an Object, itself allocated by base-loader.sys.mjs at line 155.
This is the global of the my-module.js's module, created by DevTools loader.
Then we see some more low level object up to another global object, which misses its allocation site.

# How to easily get data from try run

```bash
$ ./mach try fuzzy devtools/client/framework/test/allocations/ --query "'linux 'chrome-e10s 'opt '64-qr/opt"
```

You might also pass `--rebuild 3` if the test result is having some noise and you want more test runs.

# Following trends for these tests

You may try looking at:
<https://firefox-dev.tools/performance-dashboard/tools/memory.html>

Or at:
<https://treeherder.mozilla.org/perfherder/graphs?highlightAlerts=1&highlightChangelogData=1&series=autoland,3887143,1,12&series=mozilla-central,3887737,1,12&series=mozilla-central,3887740,1,12&series=mozilla-central,3887743,1,12&series=mozilla-central,3896204,1,12&timerange=2592000&zoom=1630504360002,1632239562424,0,123469.11111111111>

Link that you get from: <https://treeherder.mozilla.org/perfherder/graphs>
by looking at last year data for "DevTools" in the first dropdown,
and double clicking on the relevant line in "Tests" menulist.

Significant improvements and regressions will be notified through [the following dashboard](https://treeherder.mozilla.org/perfherder/alerts?hideDwnToInv=1&page=1&framework=12).
