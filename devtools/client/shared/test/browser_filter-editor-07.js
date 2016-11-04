/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget's remove button

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/locale/filterwidget.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const TEST_URI = `data:text/html,<div id="filter-container" />`;

add_task(function* () {
  let [host, win, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  let widget = new CSSFilterEditorWidget(container, "blur(2px) contrast(200%)", cssIsValid);

  info("Test removing filters with remove button");
  widget.el.querySelector(".filter button").click();

  is(widget.getCssValue(), "contrast(200%)",
     "Should remove the clicked filter");
});
