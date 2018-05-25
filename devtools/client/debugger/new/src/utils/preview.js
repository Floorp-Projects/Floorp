"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isImmutablePreview = isImmutablePreview;
exports.isImmutable = isImmutable;
exports.isReactComponent = isReactComponent;
exports.isConsole = isConsole;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const IMMUTABLE_FIELDS = ["_root", "__ownerID", "__altered", "__hash"];
const REACT_FIELDS = ["_reactInternalInstance", "_reactInternalFiber"];

function isImmutablePreview(result) {
  return result && isImmutable(result.preview);
}

function isImmutable(result) {
  if (!result || typeof result.ownProperties != "object") {
    return;
  }

  const ownProperties = result.ownProperties;
  return IMMUTABLE_FIELDS.every(field => Object.keys(ownProperties).includes(field));
}

function isReactComponent(result) {
  if (!result || typeof result.ownProperties != "object") {
    return;
  }

  const ownProperties = result.ownProperties;
  return REACT_FIELDS.some(field => Object.keys(ownProperties).includes(field));
}

function isConsole(expression) {
  return /^console/.test(expression);
}