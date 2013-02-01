<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Dietrich Ayala [dietrich@mozilla.com]  -->

The `clipboard` module allows callers to interact with the system clipboard,
setting and retrieving its contents.

You can optionally specify the type of the data to set and retrieve.
The following types are supported:

* `text` (plain text)
* `html` (a string of HTML)
* `image` (a base-64 encoded png)

If no data type is provided, then the module will detect it for you.

Currently `image`'s type doesn't support transparency on Windows.

Examples
--------

Set and get the contents of the clipboard.

    var clipboard = require("sdk/clipboard");
    clipboard.set("Lorem ipsum dolor sit amet");
    var contents = clipboard.get();

Set the clipboard contents to some HTML.

    var clipboard = require("sdk/clipboard");
    clipboard.set("<blink>Lorem ipsum dolor sit amet</blink>", "html");

If the clipboard contains HTML content, open it in a new tab.

    var clipboard = require("sdk/clipboard");
    if (clipboard.currentFlavors.indexOf("html") != -1)
      require("sdk/tabs").open("data:text/html;charset=utf-8," + clipboard.get("html"));

Set the clipboard contents to an image.

    var clipboard = require("sdk/clipboard");
    clipboard.set("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYA" +
                  "AABzenr0AAAASUlEQVRYhe3O0QkAIAwD0eyqe3Q993AQ3cBSUKpygfsNTy" +
                  "N5ugbQpK0BAADgP0BRDWXWlwEAAAAAgPsA3rzDaAAAAHgPcGrpgAnzQ2FG" +
                  "bWRR9AAAAABJRU5ErkJggg%3D%3D");

If the clipboard contains an image, open it in a new tab.

    var clipboard = require("sdk/clipboard");
    if (clipboard.currentFlavors.indexOf("image") != -1)
      require("sdk/tabs").open(clipboard.get());

As noted before, data type can be easily omitted for images.

If the intention is set the clipboard to a data URL as string and not as image,
it can be easily done specifying a different flavor, like `text`.

    var clipboard = require("sdk/clipboard");

    clipboard.set("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYA" +
                  "AABzenr0AAAASUlEQVRYhe3O0QkAIAwD0eyqe3Q993AQ3cBSUKpygfsNTy" +
                  "N5ugbQpK0BAADgP0BRDWXWlwEAAAAAgPsA3rzDaAAAAHgPcGrpgAnzQ2FG" +
                  "bWRR9AAAAABJRU5ErkJggg%3D%3D", "text");

<api name="set">
@function
  Replace the contents of the user's clipboard with the provided data.
@param data {string}
  The data to put on the clipboard.
@param [datatype] {string}
  The type of the data (optional).
</api>

<api name="get">
@function
  Get the contents of the user's clipboard.
@param [datatype] {string}
  Retrieve the clipboard contents only if matching this type (optional).
  The function will return null if the contents of the clipboard do not match
  the supplied type.
</api>

<api name="currentFlavors">
@property {array}
  Data on the clipboard is sometimes available in multiple types. For example,
  HTML data might be available as both a string of HTML (the `html` type)
  and a string of plain text (the `text` type). This function returns an array
  of all types in which the data currently on the clipboard is available.
</api>
