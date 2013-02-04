<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Accessing the DOM #

This page talks about the access content scripts have to DOM objects
in the pages they are attached to.

## XRayWrapper ##

Content scripts need to be able to access DOM objects in arbitrary web
pages, but this could cause two potential security problems:

1. JavaScript values from the content script could be accessed by the page,
enabling a malicious page to steal data or call privileged methods.
2. a malicious page could redefine standard functions and properties of DOM
objects so they don't do what the add-on expects.

To deal with this, content scripts access DOM objects using
`XRayWrapper`, (also known as
[`XPCNativeWrapper`](https://developer.mozilla.org/en/XPCNativeWrapper)).
These wrappers give the user access to the native values of DOM functions
and properties, even if they have been redefined by a script.

For example: the page below redefines `window.confirm()` to return
`true` without showing a confirmation dialog:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html lang='en' xml:lang='en' xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <script>
    window.confirm = function(message) {
      return true;
    }
    &lt;/script>
  </head>
</html>

</script>

But thanks to the wrapper, a content script which calls
`window.confirm()` will get the native implementation:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");
    var data = require("sdk/self").data;

    var widget = widgets.Widget({
      id: "transfer",
      label: "Transfer",
      content: "Transfer",
      width: 100,
      onClick: function() {
        tabs.activeTab.attach({
          // native implementation of window.confirm will be used
          contentScript: "console.log(window.confirm('Transfer all my money?'));"
        });
      }
    });

    tabs.open(data.url("xray.html"));

The wrapper is transparent to content scripts: as far as the content script
is concerned, it is accessing the DOM directly. But because it's not, some
things that you might expect to work, won't. For example, if the page includes
a library like [jQuery](http://www.jquery.com), or any other page script
adds other objects to any DOM nodes, they won't be visible to the content
script. So to use jQuery you'll typically have to add it as a content script,
as in [this example](dev-guide/guides/content-scripts/reddit-example.html).

### XRayWrapper Limitations ###

There are some limitations with accessing objects through XRayWrapper.

First, XRayWrappers don't inherit from JavaScript's
[`Object`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object),
so methods like `valueOf`, `toSource`, and `watch` are not available.
This issue is being tracked as
[bug 787013](https://bugzilla.mozilla.org/show_bug.cgi?id=787013).

Second, you can't access the prototype of an object through an XRayWrapper.
Consider a script like this:

    window.HTMLElement.prototype.foo = 'bar';
    window.alert(window.document.body.foo);

Run as a normal page script, this will work fine. But if you execute it as
a content script you'll see an error like:

<pre>
TypeError: window.HTMLElement.prototype is undefined
</pre>

This issue is being tracked as
[bug 787070](https://bugzilla.mozilla.org/show_bug.cgi?id=787070).

The main effect of this is that certain features of the
[Prototype JavaScript framework](http://www.prototypejs.org/) don't work
if it is loaded as a content script. As a workaround you can
disable these features by setting
`Prototype.BrowserFeatures.SpecificElementExtensions` to `false`
in `prototype.js`:

<pre>
 if (Prototype.Browser.MobileSafari)
   Prototype.BrowserFeatures.SpecificElementExtensions = false;

+// Disable element extension in addon-sdk content scripts
+Prototype.BrowserFeatures.SpecificElementExtensions = false;
</pre>

## Adding Event Listeners ##

You can listen for DOM events in a content script just as you can in a normal
page script, but there's one important difference: if you define an event
listener by passing it as a string into
[`setAttribute()`](https://developer.mozilla.org/en/DOM/element.setAttribute),
then the listener is evaluated in the page's context, so it will not have
access to any variables defined in the content script.

For example, this content script will fail with the error "theMessage is not
defined":

    var theMessage = "Hello from content script!";

    anElement.setAttribute("onclick", "alert(theMessage);");

So using `setAttribute()` is not recommended. Instead, add a listener by
assignment to
[`onclick`](https://developer.mozilla.org/en/DOM/element.onclick) or by using
[`addEventListener()`](https://developer.mozilla.org/en/DOM/element.addEventListener),
in either case defining the listener as a function:

    var theMessage = "Hello from content script!";

    anElement.onclick = function() {
      alert(theMessage);
    };

    anotherElement.addEventListener("click", function() {
      alert(theMessage);
    });

Note that with both `onclick` assignment and `addEventListener()`, you must
define the listener as a function. It cannot be defined as a string, whether
in a content script or in a page script.

## unsafeWindow ##

If you really need direct access to the underlying DOM, you can use the
global `unsafeWindow` object.

To see the difference, try editing the example above
so the content script uses `unsafeWindow.confirm()` instead of
`window.confirm()`.

Avoid using `unsafeWindow` if possible: it is the same concept as
Greasemonkey's unsafeWindow, and the
[warnings for that](http://wiki.greasespot.net/UnsafeWindow) apply equally
here. Also, `unsafeWindow` isn't a supported API, so it could be removed or
changed in a future version of the SDK.
