/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget's remove button

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");
const {getClientCssProperties} = require("devtools/shared/fronts/css-properties");

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  const cssIsValid = getClientCssProperties().getValidityChecker(doc);

  const container = doc.querySelector("#filter-container");
  let widget = new CSSFilterEditorWidget(
    container, "blur(2px) contrast(200%)", cssIsValid
  );

  info("Test removing filters with remove button");
  widget.el.querySelector(".filter button").click();

  is(widget.getCssValue(), "contrast(200%)",
     "Should remove the clicked filter");
});
