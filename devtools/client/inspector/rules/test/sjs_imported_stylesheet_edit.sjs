/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";
const INITIAL_CONTENT = `
div {
  color: red
}
`;

const UPDATED_CONTENT = `
span {
  color: green;
}

a {
  color: blue;
}

div {
  color: gold;
}
`;

/**
 * This sjs file supports three endpoint:
 * - "sjs_imported_stylesheet_edit.sjs" -> will return a text/css content which
 *   will be either INITIAL_CONTENT or UPDATED_CONTENT. Initially will return
 *   INITIAL_CONTENT.
 * - "sjs_imported_stylesheet_edit.sjs?update-stylesheet" -> will update an
 *   internal flag. After calling this URL, the regular endpoint will return
 *   UPDATED_CONTENT instead of INITIAL_CONTENT
 * - "sjs_imported_stylesheet_edit.sjs?setup" -> set the internal flag to its
 *   default value. Should be called at the beginning of every test to avoid
 *   side effects.
 */
function handleRequest(request, response) {
  const { queryString } = request;
  if (queryString === "setup") {
    setState("serve-updated-content", "false");
    response.setHeader("Content-Type", "text/html");
    response.write("OK");
  } else if (queryString === "update-stylesheet") {
    setState("serve-updated-content", "true");
    response.setHeader("Content-Type", "text/html");
    response.write("OK");
  } else {
    response.setHeader("Content-Type", "text/css");
    const shouldServeUpdatedCSS = getState("serve-updated-content") == "true";
    response.write(shouldServeUpdatedCSS ? UPDATED_CONTENT : INITIAL_CONTENT);
  }
}
