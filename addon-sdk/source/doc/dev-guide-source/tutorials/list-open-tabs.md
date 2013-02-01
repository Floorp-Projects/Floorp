<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# List Open Tabs #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To list the open tabs, you can iterate over the
[`tabs`](modules/sdk/tabs.html) object itself.

The following add-on adds a
[`widget`](modules/sdk/widget.html) that logs
the URLs of open tabs when the user clicks it:

    var widget = require("sdk/widget").Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: listTabs
    });

    function listTabs() {
      var tabs = require("sdk/tabs");
      for each (var tab in tabs)
        console.log(tab.url);
    }

If you run the add-on, load a couple of tabs, and click the
widget, you'll see output in the
[console](dev-guide/console.html) that looks like this:

<pre>
info: http://www.mozilla.org/en-US/about/
info: http://www.bbc.co.uk/
</pre>

You don't get direct access to any content hosted in the tab.
To access tab content you need to attach a script to the tab
using `tab.attach()`. This add-on attaches a script to all open
tabs. The script adds a red border to the tab's document:

    var widget = require("sdk/widget").Widget({
      id: "mozilla-link",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: listTabs
    });

    function listTabs() {
      var tabs = require("sdk/tabs");
      for each (var tab in tabs)
        runScript(tab);
    }

    function runScript(tab) {
      tab.attach({
        contentScript: "document.body.style.border = '5px solid red';"
      });
    }

## Learning More ##

To learn more about working with tabs in the SDK, see the
[`tabs` API reference](modules/sdk/tabs.html).

To learn more about running scripts in tabs, see the
[tutorial on using `tab.attach()`](dev-guide/tutorials/modifying-web-pages-tab.html).
