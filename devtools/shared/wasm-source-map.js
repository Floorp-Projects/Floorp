/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * SourceMapConsumer for WebAssembly source maps. It transposes columns with
 * lines, which allows mapping data to be used with SpiderMonkey Debugger API.
 */
class WasmRemap {
  /**
   * @param map SourceMapConsumer
   */
  constructor(map) {
    this._map = map;
    this.version = map.version;
    this.file = map.file;
    this._computeColumnSpans = false;
  }

  get sources() {
    return this._map.sources;
  }

  get sourceRoot() {
    return this._map.sourceRoot;
  }

  /**
   * @param url string
   */
  set sourceRoot(url) { // important, since sources are using this.
    this._map.sourceRoot = url;
  }

  get names() {
    return this._map.names;
  }

  get sourcesContent() {
    return this._map.sourcesContent;
  }

  get mappings() {
    throw new Error("not supported");
  }

  computeColumnSpans() {
    this._computeColumnSpans = true;
  }

  originalPositionFor(generatedPosition) {
    const result = this._map.originalPositionFor({
      line: 1,
      column: generatedPosition.line,
      bias: generatedPosition.bias
    });
    return result;
  }

  _remapGeneratedPosition(position) {
    const generatedPosition = {
      line: position.column,
      column: 0,
    };
    if (this._computeColumnSpans) {
      generatedPosition.lastColumn = Infinity;
    }
    return generatedPosition;
  }

  generatedPositionFor(originalPosition) {
    const position = this._map.generatedPositionFor(originalPosition);
    return this._remapGeneratedPosition(position);
  }

  allGeneratedPositionsFor(originalPosition) {
    const positions = this._map.allGeneratedPositionsFor(originalPosition);
    return positions.map((position) => {
      return this._remapGeneratedPosition(position);
    });
  }

  hasContentsOfAllSources() {
    return this._map.hasContentsOfAllSources();
  }

  sourceContentFor(source, returnNullOnMissing) {
    return this._map.sourceContentFor(source, returnNullOnMissing);
  }

  eachMapping(callback, context, order) {
    this._map.eachMapping((entry) => {
      const {
        source,
        generatedColumn,
        originalLine,
        originalColumn,
        name
      } = entry;
      callback({
        source,
        generatedLine: generatedColumn,
        generatedColumn: 0,
        originalLine,
        originalColumn,
        name,
      });
    }, context, order);
  }
}

exports.WasmRemap = WasmRemap;
