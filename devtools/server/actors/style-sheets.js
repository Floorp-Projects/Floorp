/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  styleSheetsSpec,
} = require("resource://devtools/shared/specs/style-sheets.js");

const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");

loader.lazyRequireGetter(
  this,
  "UPDATE_GENERAL",
  "resource://devtools/server/actors/utils/stylesheets-manager.js",
  true
);

/**
 * Creates a StyleSheetsActor. StyleSheetsActor provides remote access to the
 * stylesheets of a document.
 */
class StyleSheetsActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, styleSheetsSpec);

    this.parentActor = targetActor;
  }

  /**
   * The window we work with, taken from the parent actor.
   */
  get window() {
    return this.parentActor.window;
  }

  /**
   * The current content document of the window we work with.
   */
  get document() {
    return this.window.document;
  }

  getTraits() {
    return {
      traits: {},
    };
  }

  destroy() {
    for (const win of this.parentActor.windows) {
      // This flag only exists for devtools, so we are free to clear
      // it when we're done.
      win.document.styleSheetChangeEventsEnabled = false;
    }

    super.destroy();
  }

  /**
   * Create a new style sheet in the document with the given text.
   * Return an actor for it.
   *
   * @param  {object} request
   *         Debugging protocol request object, with 'text property'
   * @param  {string} fileName
   *         If the stylesheet adding is from file, `fileName` indicates the path.
   * @return {object}
   *         Object with 'styelSheet' property for form on new actor.
   */
  async addStyleSheet(text, fileName = null) {
    const styleSheetsManager = this._getStyleSheetsManager();
    await styleSheetsManager.addStyleSheet(this.document, text, fileName);
  }

  _getStyleSheetsManager() {
    return this.parentActor.getStyleSheetsManager();
  }

  toggleDisabled(resourceId) {
    const styleSheetsManager = this._getStyleSheetsManager();
    return styleSheetsManager.toggleDisabled(resourceId);
  }

  async getText(resourceId) {
    const styleSheetsManager = this._getStyleSheetsManager();
    const text = await styleSheetsManager.getText(resourceId);
    return new LongStringActor(this.conn, text || "");
  }

  update(resourceId, text, transition, cause = "") {
    const styleSheetsManager = this._getStyleSheetsManager();
    return styleSheetsManager.setStyleSheetText(resourceId, text, {
      transition,
      kind: UPDATE_GENERAL,
      cause,
    });
  }
}

exports.StyleSheetsActor = StyleSheetsActor;
