<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Display a Popup #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

To display a popup dialog, use the
[`panel`](modules/sdk/panel.html) module. A panel's content is
defined using HTML. You can run content scripts in the panel: although the
script running in the panel can't directly access your main add-on code,
you can exchange messages between the panel script and the add-on code.

<img class="image-right" src="static-files/media/screenshots/text-entry-panel.png"
alt="Text entry panel">

In this tutorial we'll create an add-on that
[adds a widget to the toolbar](dev-guide/tutorials/adding-toolbar-button.html)
which displays a panel when clicked.

The panel just contains a
`<textarea>` element: when the user presses the `return` key, the contents
of the `<textarea>` is sent to the main add-on code.

The main add-on code
[logs the message to the console](dev-guide/tutorials/logging.html).

The add-on consists of three files:

* **`main.js`**: the main add-on code, that creates the widget and panel
* **`get-text.js`**: the content script that interacts with the panel content
* **`text-entry.html`**: the panel content itself, specified as HTML

<div style="clear:both"></div>

The "main.js" looks like this:

    var data = require("sdk/self").data;

    // Construct a panel, loading its content from the "text-entry.html"
    // file in the "data" directory, and loading the "get-text.js" script
    // into it.
    var text_entry = require("sdk/panel").Panel({
      width: 212,
      height: 200,
      contentURL: data.url("text-entry.html"),
      contentScriptFile: data.url("get-text.js")
    });

    // Create a widget, and attach the panel to it, so the panel is
    // shown when the user clicks the widget.
    require("sdk/widget").Widget({
      label: "Text entry",
      id: "text-entry",
      contentURL: "http://www.mozilla.org/favicon.ico",
      panel: text_entry
    });

    // When the panel is displayed it generated an event called
    // "show": we will listen for that event and when it happens,
    // send our own "show" event to the panel's script, so the
    // script can prepare the panel for display.
    text_entry.on("show", function() {
      text_entry.port.emit("show");
    });

    // Listen for messages called "text-entered" coming from
    // the content script. The message payload is the text the user
    // entered.
    // In this implementation we'll just log the text to the console.
    text_entry.port.on("text-entered", function (text) {
      console.log(text);
      text_entry.hide();
    });

The content script "get-text.js" looks like this:

    // When the user hits return, send the "text-entered"
    // message to main.js.
    // The message payload is the contents of the edit box.
    var textArea = document.getElementById("edit-box");
    textArea.addEventListener('keyup', function onkeyup(event) {
      if (event.keyCode == 13) {
        // Remove the newline.
        text = textArea.value.replace(/(\r\n|\n|\r)/gm,"");
        self.port.emit("text-entered", text);
        textArea.value = '';
      }
    }, false);

    // Listen for the "show" event being sent from the
    // main add-on code. It means that the panel's about
    // to be shown.
    //
    // Set the focus to the text area so the user can
    // just start typing.
    self.port.on("show", function onShow() {
      textArea.focus();
    });

Finally, the "text-entry.html" file defines the `<textarea>` element:

<pre class="brush: html">

&lt;html&gt;

  &lt;head&gt;
    &lt;style type="text/css" media="all"&gt;
      textarea {
        margin: 10px;
      }
    &lt;/style&gt;
  &lt;/head&gt;

  &lt;body&gt;
    &lt;textarea rows="10" cols="20" id="edit-box">&lt;/textarea&gt;
  &lt;/body&gt;

&lt;/html&gt;

</pre>

Try it out: "main.js" is saved in your add-on's `lib` directory,
and the other two files go in your add-on's `data` directory:

<pre>
my-addon/
         data/
              get-text.js
              text-entry.html
         lib/
             main.js
</pre>

Run the add-on, click the widget, and you should see the panel.
Type some text and press "return" and you should see the output
in the console.

## Learning More ##

To learn more about the `panel` module, see the
[`panel` API reference](modules/sdk/panel.html).

To learn more about attaching panels to widgets, see the
[`widget` API reference](modules/sdk/widget.html).
