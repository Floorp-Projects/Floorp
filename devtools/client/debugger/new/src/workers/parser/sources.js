"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.hasSource = hasSource;
exports.setSource = setSource;
exports.getSource = getSource;
exports.clearSources = clearSources;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let cachedSources = new Map();

function hasSource(sourceId) {
  return cachedSources.has(sourceId);
}

function setSource(source) {
  cachedSources.set(source.id, source);
}

function getSource(sourceId) {
  if (!cachedSources.has(sourceId)) {
    throw new Error(`Parser: source ${sourceId} was not provided.`);
  }

  return cachedSources.get(sourceId);
}

function clearSources() {
  cachedSources = new Map();
}