<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# console #

The `console` object enables your add-on to log messages. If you have started
Firefox for your add-on from the command line with `cfx run` or `cfx test`
then these messages appear in the command shell you used. If the add-on has
been installed in Firefox, then the messages appear in the host application's
[Error Console](https://developer.mozilla.org/en/Error_Console).

If you're developing your add-on using the
[Add-on Builder](https://builder.addons.mozilla.org/) or are using
the [Extension Auto-installer](https://addons.mozilla.org/en-US/firefox/addon/autoinstaller/),
then the add-on is installed in Firefox, meaning that messages will appear in
the Error Console. But see the discussion of
[logging levels](dev-guide/console.html#Logging Levels): by default, messages
logged using  `log()`, `info()`, `trace()`, or `warn()` won't be logged in
these situations.

## Console Methods ##

All console methods except `exception()` and `trace()` accept one or
more JavaScript objects as arguments and log them to the console.
Depending on the console's underlying implementation and user interface,
you may be able to examine the properties of non-primitive objects
that are logged.

### <code>console.log(*object*[, *object*, ...])</code> ###

Logs the arguments to the console, preceded by "info:" and the name of your
add-on:

    console.log("This is an informational message");

<pre>
info: my-addon: This is an informational message
</pre>

### <code>console.info(*object*[, *object*, ...])</code> ###

A synonym for `console.log()`.

### <code>console.warn(*object*[, *object*, ...])</code> ###

Logs the arguments to the console, preceded by "warn:" and the name of your
add-on:

    console.warn("This is a warning message");

<pre>
warn: my-addon: This is a warning message
</pre>

### <code>console.error(*object*[, *object*, ...])</code> ###

Logs the arguments to the console, preceded by "error:" and the name of your
add-on:

    console.error("This is an error message");

<pre>
error: my-addon: This is an error message
</pre>

### <code>console.debug(*object*[, *object*, ...])</code> ###

Logs the arguments to the console, preceded by "debug:" and the name of your
add-on:

    console.error("This is a debug message");

<pre>
debug: my-addon: This is a debug message
</pre>

### <code>console.exception(*exception*)</code> ###

Logs the given exception instance as an error, outputting information
about the exception's stack traceback if one is available.

    try {
       doThing();
    } catch (e) {
       console.exception(e);
    }

    function UserException(message) {
       this.message = message;
       this.name = "UserException";
    }

    function doThing() {
      throw new UserException("Thing could not be done!");
    }

<pre>
error: my-addon: An exception occurred.
UserException: Thing could not be done!
</pre>

### <code>console.trace()</code> ###

Logs a stack trace at the point the function is called.

<h2 id="Logging Levels">Logging Levels</h2>

Logging's useful, of course, especially during development. But the more
logging there is, the more noise you see in the console output.
Especially when debug logging shows up in a production environment, the
noise can make it harder, not easier, to debug issues.

This is the problem that logging levels are designed to fix. The console
defines a number of logging levels, from "more verbose" to "less verbose",
and a number of different logging functions that correspond to these levels,
which are arranged in order of "severity" from informational
messages, through warnings, to errors.

At a given logging level, only calls to the corresponding functions and
functions with a higher severity will have any effect.

For example, if the logging level is set to "info", then calls to `info()`,
`log()`, `warn()`, and `error()` will all result in output being written.
But if the logging level is "warn" then only calls to `warn()` and `error()`
have any effect, and calls to `info()` and `log()` are simply discarded.

This means that the same code can be more verbose in a development
environment than in a production environment - you just need to arrange for
the appropriate logging level to be set.

The complete set of logging levels is given in the table below, along
with the set of functions that will result in output at each level:

<table>
  <colgroup>
    <col width="10%">
    <col width="90%">
  </colgroup>

  <tr>
    <th>Level</th>
    <th>Will log calls to:</th>
  </tr>

  <tr>
    <td>all</td>
    <td>Any console method</td>
  </tr>

  <tr>
    <td>debug</td>
    <td><code>debug()</code>, <code>log()</code>, <code>info()</code>, <code>trace()</code>, <code>warn()</code>, <code>exception()</code>, <code>error()</code></td>
  </tr>

  <tr>
    <td>info</td>
    <td><code>log()</code>, <code>info()</code>, <code>trace()</code>, <code>warn()</code>, <code>exception()</code>, <code>error()</code></td>
  </tr>

  <tr>
    <td>warn</td>
    <td><code>warn()</code>, <code>exception()</code>, <code>error()</code></td>
  </tr>

  <tr>
    <td>error</td>
    <td><code>exception()</code>, <code>error()</code></td>
  </tr>

  <tr>
    <td>off</td>
    <td>Nothing</td>
  </tr>

</table>

### Setting the Logging Level ###

The logging level defaults to "error".

There are two system preferences that can be used to override this default:

* **extensions.sdk.console.logLevel**: if set, this determines the logging
level for all installed SDK-based add-ons.

* **extensions.[extension-id].sdk.console.logLevel**: if set, this determines
the logging level for the specified add-on. This overrides the global
preference if both are set.

Both these preferences can be set programmatically using the
[`preferences/service`](modules/sdk/preferences/service.html) API, or manually
using [about:config](http://kb.mozillazine.org/About:config). The value for each
preference is the desired logging level, given as a string. 

When you run your add-on using `cfx run` or `cfx test`, the global
**extensions.sdk.console.logLevel** preference is automatically set to "info".
This means that calls to `console.log()` will appear in the console output.

When you install an add-on into Firefox, the logging level will be "error"
by default (that is, unless you have set one of the two preferences). This
means that messages written using `debug()`, `log()`, `info()`, `trace()`,
and `warn()` will not appear in the console.

This includes add-ons being developed using the
[Add-on Builder](https://builder.addons.mozilla.org/) or the
[Extension Auto-installer](https://addons.mozilla.org/en-US/firefox/addon/autoinstaller/).
