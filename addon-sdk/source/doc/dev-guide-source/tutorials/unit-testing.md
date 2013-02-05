<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Unit Testing #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html),
learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html),
and followed the tutorial on
[writing reusable modules](dev-guide/tutorials/reusable-modules.html).
</span>

The SDK provides a framework to help creates and run unit tests for
your code. To demonstrate how it works we'll write some unit tests for
a simple [Base64](http://en.wikipedia.org/wiki/Base64) encoding module.

## A Simple Base64 Module ##

In a web page, you can perform Base64 encoding and decoding using the
`btoa()` and `atob()` functions. Unfortunately these functions are attached
to the `window` object: since this object is not available in your
main add-on code, `atob()` and `btoa()` aren't available either. Using the
low-level
[window-utils](modules/sdk/deprecated/window-utils.html) module you
can access `window`, enabling you to call these functions.

However, it's good practice to encapsulate the code that directly accesses
`window-utils` in its own module, and only export the `atob()`
and `btoa()` functions. So we'll create a Base64 module to do
exactly that.

To begin with, create a new directory, navigate to it, and run `cfx init`.
Now create a new file in "lib" called "base64.js", and give it the
following contents:

    var window = require("sdk/window/utils").getMostRecentBrowserWindow();

    exports.atob = function(a) {
      return window.atob(a);
    }

    exports.btoa = function(b) {
      return window.btoa(b);
    }

This code exports two functions, which just call the corresponding
functions on the `window` object. To show the module in use, edit
the "main.js" file as follows:

    var widgets = require("sdk/widget");
    var base64 = require("./base64");

    var widget = widgets.Widget({
      id: "base64",
      label: "Base64 encode",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        encoded = base64.btoa("hello");
        console.log(encoded);
        decoded = base64.atob(encoded);
        console.log(decoded);
      }
    });

Now "main.js" imports the base64 module and calls its two exported
functions. If we run the add-on and click the widget, we should see
the following logging output:

<pre>
info: aGVsbG8=
info: hello
</pre>

## Testing the Base64 Module ##

Navigate to the add-on's `test` directory and delete the `test-main.js` file.
In its place create a file called `test-base64.js` with the following
contents:

<pre><code>
var base64 = require("./base64");

exports["test atob"] = function(assert) {
	  assert.ok(base64.atob("aGVsbG8=") == "hello", "atob works");
}

exports["test btoa"] = function(assert) {
  assert.ok(base64.btoa("hello") == "aGVsbG8=", "btoa works");
}

exports["test empty string"] = function(assert) {
  assert.throws(function() {
	              base64.atob();
	            },
                "empty string check works");
}

require("sdk/test").run(exports);
</code></pre>

This file: exports three functions, each of which expects to receive a single
argument which is an `assert` object. `assert` is supplied by the
[`test/assert`](modules/sdk/test/assert.html) module and implements
the [CommonJS Unit Testing specification](http://wiki.commonjs.org/wiki/Unit_Testing/1.1).

* The first two functions call `atob()` and `btoa()` and use
[`assert.ok()`](modules/sdk/test/assert.html)
to check that the output is as expected.

* The second function tests the module's error-handling code by passing an
empty string into `atob()` and using
[`assert.throws()`](modules/sdk/test/assert.html)
to check that the expected exception is raised.

At this point your add-on ought to look like this:

<pre>
  /base64
      package.json
      README.md
      /doc
          main.md
      /lib
          main.js
          base64.js
      /test
          test-base64.js
</pre>

Now execute `cfx --verbose test` from the add-on's root directory.
You should see something like this:

<pre>
Running tests on Firefox 13.0/Gecko 13.0 ({ec8030f7-c20a-464f-9b0e-13a3a9e97384}) under darwin/x86.
info: executing 'test-base64.test atob'
info: pass: atob works
info: executing 'test-base64.test btoa'
info: pass: btoa works
info: executing 'test-base64.test empty string'
info: pass: empty string check works

3 of 3 tests passed.
Total time: 5.172589 seconds
Program terminated successfully.
</pre>

What happens here is that `cfx test`:

<span class="aside">Note the hyphen after "test" in the module name.
`cfx test` will include a module called "test-myCode.js", but will exclude
modules called "test_myCode.js" or "testMyCode.js".</span>

* looks in the `test` directory of your
package
* loads any modules whose names start with the word `test-`
*  calls each exported function whose name starts with "test", passing it
an [`assert`](modules/sdk/test/assert.html) object as its only argument.

Obviously, you don't have to pass the `--verbose` option to `cfx` if you don't
want to; doing so just makes the output easier to read.
