/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the Filter Editor Widget parses filter values correctly (setCssValue)

const TEST_URI = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

add_task(function *() {
  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  const container = doc.querySelector("#container");
  let widget = new CSSFilterEditorWidget(container, "none");

  info("Test parsing of a valid CSS Filter value");
  widget.setCssValue("blur(2px) contrast(200%)");
  is(widget.getCssValue(),
     "blur(2px) contrast(200%)",
     "setCssValue should work for computed values");

  info("Test parsing of space-filled value");
  widget.setCssValue("blur(   2px  )   contrast(  2  )");
  is(widget.getCssValue(),
     "blur(2px) contrast(200%)",
     "setCssValue should work for spaced values");

  info("Test parsing of string-typed values");
  widget.setCssValue("drop-shadow( 2px  1px 5px black) url( example.svg#filter )");

  is(widget.getCssValue(),
     "drop-shadow(2px  1px 5px black) url(example.svg#filter)",
     "setCssValue should work for string-typed values");
});
