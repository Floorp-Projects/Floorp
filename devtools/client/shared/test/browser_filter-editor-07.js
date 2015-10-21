/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget's remove button

const TEST_URI = "chrome://devtools/content/shared/widgets/filter-frame.xhtml";

const { Cu } = require("chrome");
const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

const { ViewHelpers } = Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm", {});
const STRINGS_URI = "chrome://browser/locale/devtools/filterwidget.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

add_task(function*() {
  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  const container = doc.querySelector("#container");
  let widget = new CSSFilterEditorWidget(container, "blur(2px) contrast(200%)");

  info("Test removing filters with remove button");
  widget.el.querySelector(".filter button").click();

  is(widget.getCssValue(), "contrast(200%)",
     "Should remove the clicked filter");
});
