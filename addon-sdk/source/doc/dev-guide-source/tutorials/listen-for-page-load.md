<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Listen For Page Load #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

You can get notifications about new pages loading using the
[`tabs`](modules/sdk/tabs.html) module. The following add-on
listens to the tab's built-in `ready` event and just logs the URL of each
tab as the user loads it:

    require("sdk/tabs").on("ready", logURL);

    function logURL(tab) {
      console.log(tab.url);
    }

You don't get direct access to any content hosted in the tab.

To access tab content you need to attach a script to the tab
using `tab.attach()`. This add-on attaches a script to all open
tabs. The script adds a red border to the tab's document:

    require("sdk/tabs").on("ready", logURL);

    function logURL(tab) {
      runScript(tab);
    }

    function runScript(tab) {
      tab.attach({
        contentScript: "if (document.body) document.body.style.border = '5px solid red';"
      });
    }

(This example is only to show the idea: to implement something like this,
you should instead use
[`page-mod`](dev-guide/tutorials/modifying-web-pages-url.html),
and specify "*" as the match-pattern.)

## Learning More ##

To learn more about working with tabs in the SDK, see the
[`tabs` API reference](modules/sdk/tabs.html). You can listen
for a number of other tab events, including `open`, `close`, and `activate`.

To learn more about running scripts in tabs, see the
[tutorial on using `tab.attach()`](dev-guide/tutorials/modifying-web-pages-tab.html).
