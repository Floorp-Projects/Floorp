// Left in this test to ensure it does not cause any issues.
// eslint-disable-next-line strict
"use strict";

import setBodyText from "chrome://mochitests/content/browser/dom/tests/browser/file_module_loaded2.mjs";

document.addEventListener("DOMContentLoaded", () => {
  setBodyText();
});
