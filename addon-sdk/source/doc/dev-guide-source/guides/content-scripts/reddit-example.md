<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Reddit Example #

This example add-on creates a panel containing the mobile version of Reddit.
When the user clicks on the title of a story in the panel, the add-on opens
the linked story in a new tab in the main browser window.

To accomplish this the add-on needs to run a content script in the context of
the Reddit page which intercepts mouse clicks on each title link and fetches the
link's target URL. The content script then needs to send the URL to the add-on
script.

This is the complete add-on script:

    var data = require("sdk/self").data;

    var reddit_panel = require("sdk/panel").Panel({
      width: 240,
      height: 320,
      contentURL: "http://www.reddit.com/.mobile?keep_extension=True",
      contentScriptFile: [data.url("jquery-1.4.4.min.js"),
                          data.url("panel.js")]
    });

    reddit_panel.port.on("click", function(url) {
      require("sdk/tabs").open(url);
    });

    require("sdk/widget").Widget({
      id: "open-reddit-btn",
      label: "Reddit",
      contentURL: "http://www.reddit.com/static/favicon.ico",
      panel: reddit_panel
    });

This code supplies two content scripts to the panel's constructor in the
`contentScriptFile` option: the jQuery library and the script that intercepts
link clicks.

Finally, it registers a listener to the user-defined `click` event which in
turn passes the URL into the `open` function of the
[tabs](modules/sdk/tabs.html) module.

This is the `panel.js` content script that intercepts link clicks:

    $(window).click(function (event) {
      var t = event.target;

      // Don't intercept the click if it isn't on a link.
      if (t.nodeName != "A")
        return;

      // Don't intercept the click if it was on one of the links in the header
      // or next/previous footer, since those links should load in the panel itself.
      if ($(t).parents('#header').length || $(t).parents('.nextprev').length)
        return;

      // Intercept the click, passing it to the addon, which will load it in a tab.
      event.stopPropagation();
      event.preventDefault();
      self.port.emit('click', t.toString());
    });

This script uses jQuery to interact with the DOM of the page and the
`self.port.emit` function to pass URLs back to the add-on script.

See the `examples/reddit-panel` directory for the complete example (including
the content script containing jQuery).
