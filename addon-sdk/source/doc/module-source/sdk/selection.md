<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Eric H. Jung [eric.jung@yahoo.com] -->
<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

The `selection` module provides a means to get and set text and HTML selections
in the current Firefox page.  It can also observe new selections.

Registering for Selection Notifications
---------------------------------------

To be notified when the user makes a selection, register a listener for the
"select" event. Each listener will be called after a selection is made.

    function myListener() {
      console.log("A selection has been made.");
    }
    var selection = require("sdk/selection");
    selection.on('select', myListener);

    // You can remove listeners too.
    selection.removeListener('select', myListener);

Iterating Over Discontiguous Selections
---------------------------------------

Discontiguous selections can be accessed by iterating over the `selection`
module itself. Each iteration yields a `Selection` object from which `text`,
`html`, and `isContiguous` properties can be accessed.

## Private Windows ##

If your add-on has not opted into private browsing, then you won't see any
selections made in tabs that are hosted by private browser windows.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

Examples
--------

Log the current contiguous selection as text:

    var selection = require("sdk/selection");
    if (selection.text)
      console.log(selection.text);

Log the current discontiguous selections as HTML:

    var selection = require("sdk/selection");
    if (!selection.isContiguous) {
      for (var subselection in selection) {
         console.log(subselection.html);
      }
    }

Surround HTML selections with delimiters:

    var selection = require("sdk/selection");
    selection.on('select', function () {
      selection.html = "\\\" + selection.html + "///";
    });

<api name="text">
@property {string}
  Gets or sets the current selection as plain text. Setting the selection
  removes all current selections, inserts the specified text at the location of
  the first selection, and selects the new text. Getting the selection when
  there is no current selection returns `null`. Setting the selection when there
  is no current selection throws an exception. Getting the selection when
  `isContiguous` is `true` returns the text of the first selection.
</api>

<api name="html">
@property {string}
  Gets or sets the current selection as HTML. Setting the selection removes all
  current selections, inserts the specified text at the location of the first
  selection, and selects the new text. Getting the selection when there is no
  current selection returns `null`. Setting the selection when there is no
  current selection throws an exception. Getting the selection when
  `isContiguous` is `true` returns the text of the first selection.
</api>

<api name="isContiguous">
@property {boolean}
  `true` if the current selection is a single, contiguous selection, and `false`
  if there are two or more discrete selections, each of which may or may not be
  spatially adjacent. (Discontiguous selections can be created by the user with
  Ctrl+click-and-drag.)
</api>

<api name="select">
@event
  This event is emitted whenever the user makes a new selection in a page.
</api>
