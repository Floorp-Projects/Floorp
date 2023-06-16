/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Widget's remove button

const {
  CSSFilterEditorWidget,
} = require("resource://devtools/client/shared/widgets/FilterWidget.js");

const TEST_URI = CHROME_URL_ROOT + "doc_filter-editor-01.html";

add_task(async function () {
  const { doc } = await createHost("bottom", TEST_URI);

  const container = doc.querySelector("#filter-container");
  const widget = new CSSFilterEditorWidget(
    container,
    "blur(2px) contrast(200%)"
  );

  info("Test removing filters with remove button");
  widget.el.querySelector(".filter button").click();

  is(
    widget.getCssValue(),
    "contrast(200%)",
    "Should remove the clicked filter"
  );
  widget.destroy();
});
