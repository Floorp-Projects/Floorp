# Logging #

<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

The [DOM `console` object](https://developer.mozilla.org/en/DOM/console)
is useful for debugging JavaScript. Because DOM objects aren't available
to the main add-on code, the SDK provides its own global `console` object
with most of the same methods as the DOM `console`, including methods to
log error, warning, or informational messages. You don't have to
`require()` anything to get access to the console: it is automatically
made available to you.

The `console.log()` method prints an informational message:

    console.log("Hello World");

Try it out:

* create a new directory, and navigate to it
* execute `cfx init`
* open "lib/main.js" and add the line above
* execute `cfx run`, then `cfx run` again

Firefox will start, and the following line will appear in the command
window you used to execute `cfx run`:

<pre>
info: Hello World!
</pre>

## `console` in Content Scripts ##

You can use the console in
[content scripts](dev-guide/guides/content-scripts/index.html) as well
as in your main add-on code. The following add-on logs the HTML content
of every tab the user loads, by calling `console.log()` inside a content
script:

    require("sdk/tabs").on("ready", function(tab) {
      tab.attach({
        contentScript: "console.log(document.body.innerHTML);"
      });
    });

## `console` Output ##

If you are running your add-on from the command line (for example,
executing `cfx run` or `cfx test`) then the console's messages appear
in the command shell you used.

If you've installed the add-on in Firefox, or you're running the
add-on in the Add-on Builder, then the messages appear in Firefox's
[Error Console](https://developer.mozilla.org/en/Error_Console).

## Learning More ##

For the complete `console` API, see its
[API reference](dev-guide/console.html).
