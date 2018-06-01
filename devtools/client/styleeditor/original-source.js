/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * An object of this type represents an original source for the style
 * editor.  An "original" source is one that is mentioned in a source
 * map.
 *
 * @param {String} url
 *        The URL of the original source.
 * @param {String} sourceID
 *        The source ID of the original source, as used by the source
 *        map service.
 * @param {SourceMapService} sourceMapService
 *        The source map service; @see Toolbox.sourceMapService
 */
function OriginalSource(url, sourceId, sourceMapService) {
  this.isOriginalSource = true;

  this._url = url;
  this._sourceId = sourceId;
  this._sourceMapService = sourceMapService;
}

OriginalSource.prototype = {
  /** Get the original source's URL.  */
  get url() {
    return this._url;
  },

  /** Get the original source's URL.  */
  get href() {
    return this._url;
  },

  /**
   * Return a promise that will resolve to the original source's full
   * text.  The return result is actually an object with a single
   * `string` method; this method will return the source text as a
   * string.  This is done because the style editor elsewhere expects
   * a long string actor.
   */
  getText: function() {
    if (!this._sourcePromise) {
      this._sourcePromise = this._sourceMapService.getOriginalSourceText({
        id: this._sourceId,
        url: this._url,
      }).then(contents => {
        // Make it look like a long string actor.
        return {
          string: () => contents.text,
        };
      });
    }
    return this._sourcePromise;
  },

  /**
   * Given a source-mapped, generated style sheet, a line, and a
   * column, return the corresponding original location in this style
   * sheet.
   *
   * @param {StyleSheetFront} relatedSheet
   *        The generated style sheet's actor
   * @param {Number} line
   *        Line number.
   * @param {Number} column
   *        Column number.
   * @return {Location}
   *        The original location, an object with at least
   *        `sourceUrl`, `source`, `styleSheet`, `line`, and `column`
   *        properties.
   */
  getOriginalLocation: function(relatedSheet, line, column) {
    const {href, nodeHref, actorID: sourceId} = relatedSheet;
    const sourceUrl = href || nodeHref;
    return this._sourceMapService.getOriginalLocation({
      sourceId,
      line,
      column,
      sourceUrl,
    }).then(location => {
      // Add some properties for the style editor.
      location.source = location.sourceUrl;
      location.styleSheet = relatedSheet;
      return location;
    });
  },

  // Dummy implementations, as we never emit an event.
  on: function() { },
  off: function() { },
};

exports.OriginalSource = OriginalSource;
