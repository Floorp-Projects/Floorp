<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Add a Context Menu Item #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To add items and submenus to the Firefox context menu, use the
[`context-menu`](modules/sdk/context-menu.html) module.

Here's an add-on that adds a new context menu item. The item is
displayed whenever something in the page is selected. When it's
clicked, the selection is sent to the main add-on code, which just
logs it:

     var contextMenu = require("sdk/context-menu");
     var menuItem = contextMenu.Item({
      label: "Log Selection",
      context: contextMenu.SelectionContext(),
      contentScript: 'self.on("click", function () {' +
                     '  var text = window.getSelection().toString();' +
                     '  self.postMessage(text);' +
                     '});',
      onMessage: function (selectionText) {
        console.log(selectionText);
      }
    });

Try it: run the add-on, load a web page, select some text and right-click.
You should see the new item appear:

<img class="image-center" src="static-files/media/screenshots/context-menu-selection.png"></img>

Click it, and the selection is
[logged to the console](dev-guide/tutorials/logging.html):

<pre>
info: elephantine lizard
</pre>

All this add-on does is to construct a context menu item. You don't need
to add it: once you have constructed the item, it is automatically added
in the correct context. The constructor in this case takes four options:
`label`, `context`, `contentScript`, and `onMessage`.

### label ###

The `label` is just the string that's displayed.

### context ###

The `context` describes the circumstances in which the item should be
shown. The `context-menu` module provides a number of simple built-in
contexts, including this `SelectionContext()`, which means: display
the item when something on the page is selected.

If these simple contexts aren't enough, you can define more sophisticated
contexts using scripts.

### contentScript ###

This attaches a script to the item. In this case the script listens for
the user to click on the item, then sends a message to the add-on containing
the selected text.

### onMessage ###

The `onMessage` property provides a way for the add-on code to respond to
messages from the script attached to the context menu item. In this case
it just logs the selected text.

So:

1. the user clicks the item
2. the content script's `click` event fires, and the content script retrieves
the selected text and sends a message to the add-on
3. the add-on's `message` event fires, and the add-on code's handler function
is passed the selected text, which it logs

## Learning More ##

To learn more about the `context-menu` module, see the
[`context-menu` API reference](modules/sdk/context-menu.html).