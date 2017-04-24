# Automated tests: writing tests

<!--TODO this file might benefit from being split in other various files. For now it's just taken from the wiki with some edits-->

## Adding a new browser chrome test

It's almost always a better idea to create a new test file rather than to add new test cases to an existing one.

This prevents test files from growing up to the point where they timeout for running too long. Test systems may be under lots of stress at time and run a lot slower than your regular local environment.

It also helps with making tests more maintainable: with many small files, it's easier to track a problem rather than in one huge file.

### Creating the new file

The first thing you need to do is create a file. This file should go next to the code it's testing, in the `tests` directory. For example, an inspector test would go into `devtools/inspector/test/`.

### Naming the new file

Naming your file is pretty important to help other people get a feeling of what it is supposed to test.
Having said that, the name shouldn't be too long either.

A good naming convention is `browser_<panel>_<short-description>[_N].js`

where:

* `<panel>` is one of `debugger`, `markupview`, `inspector`, `ruleview`, etc.
* `<short-description>` should be about 3 to 4 words, separated by hyphens (-)
* and optionally add a number at the end if you have several files testing the same thing

For example: `browser_ruleview_completion-existing-property_01.js`

Note that not all existing tests are consistently named. So the rule we try to follow is to **be consistent with how other tests in the same test folder are named**.

### Basic structure of a test

```javascript
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A detailed description of what the test is supposed to test

const TEST_URL = TEST_URL_ROOT + "doc_some_test_page.html";

add_task(function*() {
yield addTab(TEST_URL_ROOT);
let {toolbox, inspector, view} = yield openRuleView();
yield selectNode("#testNode", inspector);
yield checkSomethingFirst(view);
yield checkSomethingElse(view);
});

function* checkSomethingFirst(view) {
/* ... do something ... this function can yield */
}

function* checkSomethingElse(view) {
/* ... do something ... this function can yield */
}
```

### Referencing the new file

For your test to be run, it needs to be referenced in the `browser.ini` file that you'll find in the same directory. For example: `browser/devtools/debugger/test/browser.ini`

Add a line with your file name between square brackets, and make sure that the list of files **is always sorted by alphabetical order** (some lists can be really long, so the alphabetical order helps in finding and reasoning about things).

For example, if you were to add the test from the previous section, you'd add this to `browser.ini`:

```ini
[browser_ruleview_completion-existing-property_01.js]
```

### Adding support files

Sometimes your test may need to open an HTML file in a tab, and it may also need to load CSS or JavaScript. For this to work, you'll need to...

1. place these files in the same directory, and also 
2. reference them in the `browser.ini` file.

There's a naming convention for support files: `doc_<support-some-test>.html`

But again, often names do not follow this convention, so try to follow the style of the other support files currently in the same test directory.

To reference your new support file, add its filename in the `support-files` section of `browser.ini`, also making sure this section is in alphabetical order.

Support files can be accessed via a local server that is started while tests are running. This server is accessible at [http://example.com/browser/](http://example.com/browser/). See the `head.js section` below for more information.

## Leveraging helpers in `head.js`

`head.js` is a special support file that is loaded in the scope the test runs in, before the test starts. It contains global helpers that are useful for most tests. Read through the head.js file in your test directory to see what functions are there and therefore avoid duplicating code.

Each panel in DevTools has its own test directory with its own `head.js`, so you'll find different things in each panel's `head.js` file.

For example, the head.js files in the `markupview` and `styleinspector` test folders contain these useful functions and constants:

* Base URLs for support files: `TEST_URL_ROOT`. This avoids having to duplicate the http://example.com/browser/browser/devtools/styleinspector/ URL fragment in all tests,
* `waitForExplicitFinish()` is called in `head.js` once and for all<!--TODO: what does this even mean?-->. All tests are asynchronous, so there's no need to call it again in each and every test,
* `auto-cleanup`: the toolbox is closed automatically and all tabs are closed,
* `tab addTab(url)`
* `{toolbox, inspector} openInspector()`
* `{toolbox, inspector, view} openRuleView()`
* `selectNode(selectorOrNode, inspector)`
* `node getNode(selectorOrNode)`
* ...

## Shared head.js file

A [shared-head.js](https://dxr.mozilla.org/mozilla-central/source/devtools/client/framework/test/shared-head.js) file has been introduced to avoid duplicating code in various `head.js` files.

It's important to know whether or not the `shared.js` in your test directory already imports `shared-head.js` (look for a <code>Services.scriptloader.loadSubScript</code> call), as common helpers in `shared-head.js` might be useful for your test.

If you're planning to work on a lot of new tests, it might be worth the time actually importing `shared-head.js` in your `head.js` if it isn't here already.

## E10S (Electrolysis)

E10S is the codename for Firefox multi-process, and what that means for us is that the process in which the test runs isn't the same as the one in which the test content page runs.

You can learn more about E10S [from this blog post](https://timtaubert.de/blog/2011/08/firefox-electrolysis-101/) and [the Electrolysis wiki page](https://wiki.mozilla.org/Electrolysis).

One of the direct consequences of E10S on tests is that you cannot retrieve and manipulate objects from the content page as you'd do without E10S.

Well this isn't entirely true, because with [cross-process object wrappers](https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox/Cross_Process_Object_Wrappers CPOWs) you can somehow access the page, get to DOM nodes, and read their attributes for instance, but a lot of other things you'd expect to work without E10S won't work exactly the same or at all.

Using CPOWs is discouraged; they are only temporarily allowed in mochitests, and their use is forbidden in browser code.

So when creating a new test, if this test needs to access the content page in any way, you can use [the message manager](https://developer.mozilla.org/en-US/docs/The_message_manager) to communicate with a script loaded in the content process to do things for you instead of accessing objects in the page directly.

You can use the helper `ContentTask.spawn()` for this. See [this list of DevTools tests that use that helper for examples](https://dxr.mozilla.org/mozilla-central/search?q=ContentTask.spawn%28+path%3Adevtools%2Fclient&redirect=false&case=false).

Note that a lot of tests only need to access the DevTools UI anyway, and don't need to interact with the content process at all. Since the UI lives in the same process as the test, you won't need to use the message manager to access it.

## Asynchronous tests

Most browser chrome DevTools tests are asynchronous. One of the reasons why they are asynchronous is that the code needs to register event handlers for various user interactions in the tools and then simulate these interactions. Another reason is that most DevTools operations are done asynchronously via the debugger protocol.

Here are a few things to keep in mind with regards to asynchronous testing:

* `head.js` already calls `waitForExplicitFinish()` so there's no need for your new test to do it too.
* Using `add_task` with a generator function means that you can yield calls to functions that return promises. It also means your main test function can be written to almost look like synchronous code, by adding `yield` before calls to asynchronous functions. For example:

```javascript
for (let test of testData) {
	yield testCompletion(test, editor, view);
}
```

Each call to `testCompletion` is asynchronous, but the code doesn't need to rely on nested callbacks and maintain an index, a standard for loop can be used.

<!--TODO: I think the following paragraph might be slightly outdated-->

* Define your test functions as generators that yield, no need for them to be tasks since they are called from one already. In some cases you'll need to return promises anyway (if you're adding a new helper function to head.js for example). If this is the case, it sometimes is best to define your function like so:

```javascript
let myHelperFunction = Task.async(function*() {
	...
});
```

## Writing clean, maintainable test code

Test code is as important as feature code itself, it helps avoiding regressions of course, but it also helps understanding complex parts of the code that would be otherwise hard to grasp.

Since we find ourselves working with test code a large portion of our time, we should spend the time and energy it takes to make this time enjoyable.

### Logs and comments

Reading test output logs isn't exactly fun and it takes time but is needed at times. Make sure your test generates enough logs by using:

```
info("doing something now")
```

it helps a lot knowing around which lines the test fails, if it fails.

One good rule of thumb is if you're about to add a JS line comment in your test to explain what the code below is about to test, write the same comment in an `info()` instead.

Also add a description at the top of the file to help understand what this test is about. The file name is often not long enough to convey everything you need to know about the test. Understanding a test often teaches you about the feature itself.

Not really a comment, but don't forget to "use strict";

### Callbacks and promises

Avoid multiple nested callbacks or chained promises. They make it hard to read the code.

Thanks to `add_task`, it's easy to write asynchronous code that looks like flat, synchronous, code.

### Clean up after yourself

Do not expose global variables in your test file, they may end up causing bugs that are hard to track. Most functions in `head.js` return useful instances of the DevTools panels, and you can pass these as arguments to your sub functions, no need to store them in the global scope.
This avoids having to remember nullifying them at the end.

If your test needs to toggle user preferences, make sure you reset these preferences when the test ends. Do not reset them at the end of the test function though because if your test fails, the preferences will never be reset. Use the `registerCleanupFunction` helper instead.

It may be a good idea to do the reset in `head.js`.

### Write small, maintainable code

Split your main test function into smaller test functions with self explanatory names.

Make sure your test files are small. If you are working on a new feature, you can create a new test each time you add a new functionality, a new button to the UI for instance. This helps having small, incremental tests and can also help writing test while coding.

If your test is just a sequence of functions being called to do the same thing over and over again, it may be better to describe the test steps in an array instead and just have one function that runs each item of the array. See the following example

```javascript
const TESTS = [
	{desc: "add a class", cssSelector: "#id1", makeChanges: function*() {...}},
	{desc: "change href", cssSelector: "a.the-link", makeChanges: function*() {...}},
	...
];

add_task(function*() {
	yield addTab("...");
	let {toolbox, inspector} = yield openInspector();
	for (let step of TESTS) {
	  info("Testing step: " + step.desc);
	  yield selectNode(step.cssSelector, inspector);
	  yield step.makeChanges();
	}
});
```

As shown in this code example, you can add as many test cases as you want in the TESTS array and the actual test code will remain very short, and easy to understand and maintain (note that when looping through test arrays, it's always a good idea to add a "desc" property that will be used in an info() log output).

### Avoid exceptions

Even when they're not failing the test, exceptions are bad because they pollute the logs and make them harder to read.
They're also bad because when your test is run as part of a test suite and if an other, unrelated, test fails then the exceptions may give wrong information to the person fixing the unrelated test.

After your test has run locally, just make sure it doesn't output exceptions by scrolling through the logs.

Often, non-blocking exceptions may be caused by hanging protocol requests that haven't been responded to yet when the tools get closed at the end of the test. Make sure you register to the right events and give time to the tools to update themselves before moving on.

### Avoid test timeouts

<!--TODO: this recommendation is conflicting with the above recommendation. What? -->
When tests fail, it's far better to have them fail and end immediately with an exception that will help fix it rather than have them hang until they hit the timeout and get killed.

## Adding new helpers

In some cases, you may want to extract some common code from your test to use it another another test. 

* If this is very common code that all tests could use, then add it to `devtools/client/framework/test/shared-head.js`.
* If this is common code specific to a given tool, then add it to the corresponding `head.js` file.
* If it isn't common enough to live in `head.js`, then it may be a good idea to create a helper file to avoid duplication anyway. Here's how to create a helper file:
 * Create a new file in your test director. The naming convention should be `helper_<description_of_the_helper>.js`
 * Add it to the browser.ini support-files section, making sure it is sorted alphabetically
 * Load the helper file in the tests
 * `browser/devtools/markupview/test/head.js` has a handy `loadHelperScript(fileName)` function that you can use.
 * The file will be loaded in the test global scope, so any global function or variables it defines will be available (just like `head.js`).
 * Use the special ESLint comment `/* import-globals-from helper_file.js */` to prevent ESLint errors for undefined variables.

In all cases, new helper functions should be properly commented with an jsdoc comment block.

