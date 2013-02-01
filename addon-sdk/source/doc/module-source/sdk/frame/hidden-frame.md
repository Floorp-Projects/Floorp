<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Myk Melez [myk@mozilla.org] -->

The `hidden-frame` module creates Firefox frames (i.e. XUL `<iframe>`
elements) that are not displayed to the user. It is useful in the construction
of APIs that load web content not intended to be directly seen or accessed
by users, like
[`page-worker`](modules/sdk/page-worker.html).
It is also useful in the construction of APIs
that load web content for intermittent display, such as
[`panel`](modules/sdk/panel.html).

The module exports a constructor function, `HiddenFrame`, and two other
functions, `add` and `remove`.

`HiddenFrame` constructs a new hidden frame.  `add` registers a hidden frame,
preparing it to load content.  `remove` unregisters a frame, unloading any
content that was loaded in it.

The following code creates a hidden frame, loads a web page into it, and then
logs its title:

    var hiddenFrames = require("sdk/frame/hidden-frame");
    let hiddenFrame = hiddenFrames.add(hiddenFrames.HiddenFrame({
      onReady: function() {
        this.element.contentWindow.location = "http://www.mozilla.org/";
        let self = this;
        this.element.addEventListener("DOMContentLoaded", function() {
          console.log(self.element.contentDocument.title);
        }, true, true);
      }
    }));

See the `panel` module for a real-world example of usage of this module.

<api name="HiddenFrame">
@class
`HiddenFrame` objects represent hidden frames.
<api name="HiddenFrame">
@constructor
Creates a hidden frame.
@param options {object}
  Options for the frame, with the following keys:
  @prop onReady {function,array}
    Functions to call when the frame is ready to load content.  You must specify
    an `onReady` callback and refrain from using the hidden frame until
    the callback gets called, because hidden frames are not always ready to load
    content the moment they are added.
</api>

<api name="element">
@property {DOMElement}
The host application frame in which the page is loaded.
</api>

<api name="ready">
@event

This event is emitted when the DOM for a hidden frame content is ready.
It is equivalent to the `DOMContentLoaded` event for the content page in
a hidden frame.
</api>
</api>

<api name="add">
@function
Register a hidden frame, preparing it to load content.
@param hiddenFrame {HiddenFrame} the frame to add
</api>

<api name="remove">
@function
Unregister a hidden frame, unloading any content that was loaded in it.
@param hiddenFrame {HiddenFrame} the frame to remove
</api>
