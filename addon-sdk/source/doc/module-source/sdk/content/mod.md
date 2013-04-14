<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `mod` module provides functions to modify a page content.

<api name="attachTo">
@function
  Function applies given `modification` to a given `window`.

  For example, the following code applies a style to a content window, adding
  a border to all divs in page:

      var attachTo = require("sdk/content/mod").attachTo;
      var Style = require("sdk/stylesheet/style").Style;

      var style = Style({
        source: "div { border: 4px solid gray }"
      });

      // assuming window points to the content page we want to modify
      attachTo(style, window);

@param modification {object}
  The modification we want to apply to the target.

@param window {nsIDOMWindow}
  The window to be modified.
</api>

<api name="detachFrom">
@function
  Function removes attached `modification` from a given `window`.
  If `window` is not specified, `modification` is removed from all the windows
  it's being attached to.

  For example, the following code applies and removes a style to a content
  window, adding a border to all divs in page:

      var { attachTo, detachFrom } = require("sdk/content/mod");
      var Style = require("sdk/stylesheet/style").Style;

      var style = Style({
        source: "div { border: 4px solid gray }"
      });

      // assuming window points to the content page we want to modify
      attachTo(style, window);
      // ...
      detachFrom(style, window);

@param modification {object}
  The modification we want to remove from the target

@param window {nsIDOMWindow}
  The window to be modified.
  If `window` is not provided `modification` is removed from all targets it's
  being attached to.
</api>

<api name="getTargetWindow">
@function
  Function takes `target`, value representing content (page) and returns
  `nsIDOMWindow` for that content.
  If `target` does not represents valid content `null` is returned.
  For example target can be a content window itself in which case it's will be
  returned back.

@param target {object}
  The object for which we want to obtain the window represented or contained.
  If a `nsIDOMWindow` is given, it works as an identify function, returns
  `target` itself.
@returns {nsIDOMWindow|null}
  The window represented or contained by the `target`, if any. Returns `null`
  otherwise.
</api>

<api name="attach">
@function
  Function applies given `modification` to a given `target` representing a
  content to be modified.

@param modification {object}
  The modification we want to apply to the target

@param target {object}
  Target is a value that representing content to be modified. It is valid only
  when `getTargetWindow(target)` returns nsIDOMWindow of content it represents.
</api>

<api name="detach">
@function
  Function removes attached `modification`. If `target` is specified
  `modification` is removed from that `target` only, otherwise `modification` is
  removed from all the targets it's being attached to.

@param modification {object}
  The modification we want to remove from the target

@param target {object}
  Target is a value that representing content to be modified. It is valid only
  when `getTargetWindow(target)` returns `nsIDOMWindow` of content it represents.
  If `target` is not provided `modification` is removed from all targets it's
  being attached to.
</api>
