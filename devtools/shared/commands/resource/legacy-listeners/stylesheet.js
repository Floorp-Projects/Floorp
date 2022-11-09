/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

module.exports = async function({ targetFront, onAvailable, onUpdated }) {
  if (!targetFront.hasActor("styleSheets")) {
    return;
  }

  const onStyleSheetAdded = async (styleSheet, isNew, fileName) => {
    const onMediaRules = styleSheet.getMediaRules();
    const resource = toResource(styleSheet, isNew, fileName);

    let previousMediaRules = [];

    function updateMediaRule(index, rule) {
      onUpdated([
        {
          resourceType: resource.resourceType,
          resourceId: resource.resourceId,
          updateType: "matches-change",
          nestedResourceUpdates: [
            {
              path: ["mediaRules", index],
              value: rule,
            },
          ],
        },
      ]);
    }

    function addMatchesChangeListener(mediaRules) {
      for (const rule of previousMediaRules) {
        rule.destroy();
      }

      mediaRules.forEach((rule, index) => {
        rule.on("matches-change", matches => updateMediaRule(index, rule));
      });

      previousMediaRules = mediaRules;
    }

    styleSheet.on("style-applied", (kind, styleSheetFront, cause) => {
      onUpdated([
        {
          resourceType: resource.resourceType,
          resourceId: resource.resourceId,
          updateType: "style-applied",
          event: {
            cause,
            kind,
          },
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

    styleSheet.on("media-rules-changed", mediaRules => {
      addMatchesChangeListener(mediaRules);
      onUpdated([
        {
          resourceType: resource.resourceType,
          resourceId: resource.resourceId,
          updateType: "at-rules-changed",
          resourceUpdates: {
            atRules: mediaRules.map(mediaRuleFrontToAtRuleResourceUpdate),
          },
        },
      ]);
    });

    try {
      const mediaRules = await onMediaRules;
      resource.atRules = mediaRules.map(mediaRuleFrontToAtRuleResourceUpdate);
      addMatchesChangeListener(mediaRules);
    } catch (e) {
      // There are cases that the stylesheet front was destroyed already when/while calling
      // methods of stylesheet.
      console.warn("fetching media rules failed", e);
    }

    return resource;
  };

  const styleSheetsFront = await targetFront.getFront("stylesheets");
  try {
    const styleSheets = await styleSheetsFront.getStyleSheets();
    onAvailable(
      await Promise.all(
        styleSheets.map(styleSheet =>
          onStyleSheetAdded(styleSheet, false, null)
        )
      )
    );

    styleSheetsFront.on(
      "stylesheet-added",
      async (styleSheet, isNew, fileName) => {
        onAvailable([await onStyleSheetAdded(styleSheet, isNew, fileName)]);
      }
    );
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
    resourceType: ResourceCommand.TYPES.STYLESHEET,
    isNew,
    fileName,
  });
  return styleSheet;
}

/**
 * Returns an object with the data expected by the StyleEditor At-rules panel.
 *
 * @param {MediaRuleFront} mediaRuleFront
 * @returns {Object}
 */
function mediaRuleFrontToAtRuleResourceUpdate(mediaRuleFront) {
  return {
    type: "media",
    mediaText: mediaRuleFront.mediaText,
    conditionText: mediaRuleFront.conditionText,
    matches: mediaRuleFront.matches,
    line: mediaRuleFront.line,
    column: mediaRuleFront.column,
  };
}
