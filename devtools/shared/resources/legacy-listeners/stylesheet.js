/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

module.exports = async function({ targetFront, onAvailable, onUpdated }) {
  if (!targetFront.hasActor("styleSheets")) {
    return;
  }

  const onStyleSheetAdded = (styleSheet, isNew, fileName) => {
    const resource = toResource(styleSheet, isNew, fileName);

    styleSheet.on("style-applied", () => {
      onUpdated([
        {
          resourceType: resource.resourceType,
          resourceId: resource.resourceId,
          updateType: "style-applied",
        },
      ]);
    });

    styleSheet.on("property-change", (property, value) => {
      onUpdated([
        {
          resourceType: resource.resourceType,
          resourceId: resource.resourceId,
          updateType: "property-change",
          resourceUpdates: { [property]: value },
        },
      ]);
    });

    return resource;
  };

  const styleSheetsFront = await targetFront.getFront("stylesheets");
  try {
    const styleSheets = await styleSheetsFront.getStyleSheets();
    onAvailable(
      styleSheets.map(styleSheet => onStyleSheetAdded(styleSheet, false, null))
    );

    styleSheetsFront.on("stylesheet-added", (styleSheet, isNew, fileName) => {
      onAvailable([onStyleSheetAdded(styleSheet, isNew, fileName)]);
    });
  } catch (e) {
    // There are cases that the stylesheet front was destroyed already when/while calling
    // methods of stylesheet.
    // Especially, since source map url service starts to watch the stylesheet resources
    // lazily, the possibility will be extended.
    console.warn("fetching stylesheets failed", e);
  }
};

function toResource(styleSheet, isNew, fileName) {
  Object.assign(styleSheet, {
    resourceId: styleSheet.actorID,
    resourceType: ResourceWatcher.TYPES.STYLESHEET,
    isNew,
    fileName,
  });
  return styleSheet;
}
