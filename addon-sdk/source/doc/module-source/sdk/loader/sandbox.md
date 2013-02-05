<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Provides an API for creating JavaScript sandboxes and executing scripts
in them.

## Create a sandbox ##

To create a sandbox:

    const { sandbox, evaluate, load } = require("sdk/loader/sandbox");
    let scope = sandbox('http://example.com');

The argument passed to the sandbox defines its privileges. The argument may be:

* a URL string, in which case the sandbox will get the same privileges as
a script loaded from that URL
* a DOM window object, to inherit privileges from the window being passed.
* omitted or `null`: then the sandbox will have chrome privileges giving it
access to all the XPCOM components.

Optionally the `sandbox` function can be passed a second argument
(See [sandbox documentation on MDN](https://developer.mozilla.org/en/Components.utils.Sandbox#Optional_parameter)
for details).

## Evaluate code ##

Module provides `evaluate` function that lets you execute code in the given
sandbox:

    evaluate(scope, 'var a = 5;');
    evaluate(scope, 'a + 2;');      //=> 7

More details about evaluated script may be passed via optional arguments that
may improve exception reporting:

    // Evaluate code as if it was loaded from 'http://foo.com/bar.js' and
    // start from 2nd line.
    evaluate(scope, 'a ++', 'http://foo.com/bar.js', 2);

Version of JavaScript can be also specified via an optional argument:

    evaluate(scope, 'let b = 2;', 'bar.js', 1, '1.5');
    // throws cause `let` is not defined in JS 1.5.

## Load scripts ##

This module provides a limited API for loading scripts from local URLs.
`data:` URLs are supported.

    load(scope, 'resource://path/to/my/script.js');
    load(scope, 'file:///path/to/script.js');
    load(scope, 'data:,var a = 5;');

<api name="sandbox">
@function
  Make a new sandbox that inherits principals from `source`.
  @param source {string|window|null}
  An object that determines the privileges that will be given to the
  sandbox. This argument can be:

  * a URI string, giving the sandbox the same privileges as a
  script loaded from that URL
  * a DOM window object, giving the sandbox the same privileges
  as the DOM window
  * `null`, to give the sandbox chrome privileges.

@returns {sandbox}
  A sandbox in which you can evaluate and load JavaScript.
</api>

<api name="evaluate">
@function
  Evaluate `code` in `sandbox`, and return the result.
  @param sandbox {sandbox}
  The sandbox to use.
  @param code {string}
  The code to execute.
  @param uri {string}
  Evaluate the code as if it were being loaded from the given URI. Optional.
  @param line {number}
  Evaluate the code starting at this line. Optional, defaults to 1.
  @param version {string}
  Evaluate the code using this version of JavaScript. Defaults to 1.8.
@returns {result}
  Returns whatever the evaluated code returns.
</api>

<api name="load">
@function
  Evaluate code from `uri` in `sandbox`.
  @param sandbox {sandbox}
  The sandbox to use.
  @param uri {string}
  The URL pointing to the script to load.
  It must be a local `chrome:`, `resource:`, `file:` or `data:` URL.
@returns {result}
  Returns whatever the evaluated code returns.
</api>
