/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetFront, onAvailable }) {
  if (!targetFront.hasActor("styleSheets")) {
    return;
  }

  const styleSheetsFront = await targetFront.getFront("stylesheets");
  try {
    const styleSheets = await styleSheetsFront.getStyleSheets();
    onAvailable(styleSheets.map(styleSheet => toResource(styleSheet, false)));

    styleSheetsFront.on("stylesheet-added", (styleSheet, isNew) => {
      onAvailable([toResource(styleSheet, isNew)]);
    });
  } catch (e) {
    // There are cases that the stylesheet front was destroyed already when/while calling
    // methods of stylesheet.
    // Especially, since source map url service starts to watch the stylesheet resources
    // lazily, the possibility will be extended.
    console.warn("fetching stylesheets failed", e);
  }
};

function toResource(styleSheet, isNew) {
  return {
    resourceType: ResourceWatcher.TYPES.STYLESHEET,
    styleSheet,
    isNew,
  };
}
