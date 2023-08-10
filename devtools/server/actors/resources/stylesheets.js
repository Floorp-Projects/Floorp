/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { STYLESHEET },
} = require("resource://devtools/server/actors/resources/index.js");

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "resource://devtools/shared/inspector/css-logic.js"
);

class StyleSheetWatcher {
  constructor() {
    this._onApplicableStylesheetAdded =
      this._onApplicableStylesheetAdded.bind(this);
    this._onStylesheetUpdated = this._onStylesheetUpdated.bind(this);
    this._onStylesheetRemoved = this._onStylesheetRemoved.bind(this);
  }

  /**
   * Start watching for all stylesheets related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe css changes.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable, onUpdated, onDestroyed }) {
    this._targetActor = targetActor;
    this._onAvailable = onAvailable;
    this._onUpdated = onUpdated;
    this._onDestroyed = onDestroyed;

    this._styleSheetsManager = targetActor.getStyleSheetsManager();

    // Add event listener for new additions and updates
    this._styleSheetsManager.on(
      "applicable-stylesheet-added",
      this._onApplicableStylesheetAdded
    );
    this._styleSheetsManager.on(
      "stylesheet-updated",
      this._onStylesheetUpdated
    );
    this._styleSheetsManager.on(
      "applicable-stylesheet-removed",
      this._onStylesheetRemoved
    );

    // startWatching will emit applicable-stylesheet-added for already existing stylesheet
    await this._styleSheetsManager.startWatching();
  }

  _onApplicableStylesheetAdded(styleSheetData) {
    return this._notifyResourcesAvailable([styleSheetData]);
  }

  _onStylesheetUpdated({ resourceId, updateKind, updates = {} }) {
    this._notifyResourceUpdated(resourceId, updateKind, updates);
  }

  _onStylesheetRemoved({ resourceId }) {
    return this._notifyResourcesDestroyed(resourceId);
  }

  async _toResource(
    styleSheet,
    { isCreatedByDevTools = false, fileName = null, resourceId } = {}
  ) {
    const { atRules, ruleCount } =
      this._styleSheetsManager.getStyleSheetRuleCountAndAtRules(styleSheet);

    const resource = {
      resourceId,
      resourceType: STYLESHEET,
      disabled: styleSheet.disabled,
      constructed: styleSheet.constructed,
      fileName,
      href: styleSheet.href,
      isNew: isCreatedByDevTools,
      atRules,
      nodeHref: this._styleSheetsManager._getNodeHref(styleSheet),
      ruleCount,
      sourceMapBaseURL:
        this._styleSheetsManager._getSourcemapBaseURL(styleSheet),
      sourceMapURL: styleSheet.sourceMapURL,
      styleSheetIndex: this._styleSheetsManager._getStyleSheetIndex(styleSheet),
      system: CssLogic.isAgentStylesheet(styleSheet),
      title: styleSheet.title,
    };

    return resource;
  }

  async _notifyResourcesAvailable(styleSheets) {
    const resources = await Promise.all(
      styleSheets.map(async ({ resourceId, styleSheet, creationData }) => {
        const resource = await this._toResource(styleSheet, {
          resourceId,
          isCreatedByDevTools: creationData?.isCreatedByDevTools,
          fileName: creationData?.fileName,
        });

        return resource;
      })
    );

    await this._onAvailable(resources);
  }

  _notifyResourceUpdated(
    resourceId,
    updateType,
    { resourceUpdates, nestedResourceUpdates, event }
  ) {
    this._onUpdated([
      {
        browsingContextID: this._targetActor.browsingContextID,
        innerWindowId: this._targetActor.innerWindowId,
        resourceType: STYLESHEET,
        resourceId,
        updateType,
        resourceUpdates,
        nestedResourceUpdates,
        event,
      },
    ]);
  }

  _notifyResourcesDestroyed(resourceId) {
    this._onDestroyed([
      {
        resourceType: STYLESHEET,
        resourceId,
      },
    ]);
  }

  destroy() {
    this._styleSheetsManager.off(
      "applicable-stylesheet-added",
      this._onApplicableStylesheetAdded
    );
    this._styleSheetsManager.off(
      "stylesheet-updated",
      this._onStylesheetUpdated
    );
    this._styleSheetsManager.off(
      "applicable-stylesheet-removed",
      this._onStylesheetRemoved
    );
  }
}

module.exports = StyleSheetWatcher;
