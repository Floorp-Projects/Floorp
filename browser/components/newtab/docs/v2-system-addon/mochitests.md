# Mochitests

We use [mochitests](https://firefox-source-docs.mozilla.org/testing/mochitest-plain/) to do functional (and possibly integration) testing. Mochitests are part of Firefox and allow us to test activity stream literally as you would use it.

Mochitests live in `test/functional/mochitest`, and as of this writing, they
 are all the [`browser-chrome`](https://firefox-source-docs.mozilla.org/testing/chrome-tests/) flavor of mochitests.  They currently only run against the bootstrapped version of the add-on in system-addon, not the test pilot version at the top level directory.

## Adding New Tests

If you add new tests, make sure to list them in the `browser.ini` file. You will see the other tests there. Add a new entry with the same format as the others. You can also add new JS or HTML files by listing in under `support-files`. Make sure to start your test name with "browser_", so that the test suite knows the pick it up. E.g: "browser_as_my_new_test.js".

## Writing Tests

Here are a few tips for writing mochitests:

* Only write mochitests for testing the interaction of multiple components on the page and to make sure that the protocol is working.
* If you need to access the content page, use `ContentTask.spawn`:

```js
ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
  content.wrappedJSObject.foo();
});
```

The above calls the function `foo` that exists in the page itself. You can also access the DOM this way: `content.document.querySelector`, if you want to click a button or do other things. You can even you use assertions inside this callback to check DOM state.

* If you run into problems running tests in e10s, refer to the [wiki](https://wiki.mozilla.org/Electrolysis/e10s_test_tips) for tips
* Nobody likes to see intermittent oranges in their tests, so read the [docs on how to avoid them](https://firefox-source-docs.mozilla.org/testing/intermittent/)!
