<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->


# Loading Content Scripts #

The constructors for content-script-using objects such as panel and page-mod
define a group of options for loading content scripts:

<pre>
  contentScript         string, array
  contentScriptFile     string, array
  contentScriptWhen     string
  contentScriptOptions  object
</pre>

We have already seen the `contentScript` option, which enables you to pass
in the text of the script itself as a string literal. This version of the API
avoids the need to maintain a separate file for the content script.

The `contentScriptFile` option enables you to pass in the local file URL from
which the content script will be loaded. To supply the file
"my-content-script.js", located in the /data subdirectory under your add-on's
root directory, use a line like:

    // "data" is supplied by the "self" module
    var data = require("sdk/self").data;
    ...
    contentScriptFile: data.url("my-content-script.js")

Both `contentScript` and `contentScriptFile` accept an array of strings, so you
can load multiple scripts, which can also interact directly with each other in
the content process:

    // "data" is supplied by the "self" module
    var data = require("sdk/self").data;
    ...
    contentScriptFile:
        [data.url("jquery-1.4.2.min.js"), data.url("my-content-script.js")]

Scripts specified using contentScriptFile are loaded before those specified
using contentScript. This enables you to load a JavaScript library like jQuery
by URL, then pass in a simple script inline that can use jQuery.

<div class="warning">
<p>Unless your content script is extremely simple and consists only of a
static string, don't use <code>contentScript</code>: if you do, you may
have problems getting your add-on approved on AMO.</p>
<p>Instead, keep the script in a separate file and load it using
<code>contentScriptFile</code>. This makes your code easier to maintain,
secure, debug and review.</p>
</div>

The `contentScriptWhen` option specifies when the content script(s) should be
loaded. It takes one of three possible values:

* "start" loads the scripts immediately after the document element for the
page is inserted into the DOM. At this point the DOM content hasn't been
loaded yet, so the script won't be able to interact with it.

* "ready" loads the scripts after the DOM for the page has been loaded: that
is, at the point the
[DOMContentLoaded](https://developer.mozilla.org/en/Gecko-Specific_DOM_Events)
event fires. At this point, content scripts are able to interact with the DOM
content, but externally-referenced stylesheets and images may not have finished
loading.

* "end" loads the scripts after all content (DOM, JS, CSS, images) for the page
has been loaded, at the time the
[window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
fires.

The default value is "end".

The `contentScriptOptions` is a json that is exposed to content scripts as a read
only value under `self.options` property.

Any kind of jsonable value (object, array, string, etc.) can be used here.
