"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isImmutable = isImmutable;
exports.isReactComponent = isReactComponent;
exports.isConsole = isConsole;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const IMMUTABLE_FIELDS = ["_root", "__ownerID", "__altered", "__hash"];

function isImmutable(result) {
  if (!result || !result.preview) {
    return;
  }

  const ownProperties = result.preview.ownProperties;

  if (!ownProperties) {
    return;
  }

  return IMMUTABLE_FIELDS.every(field => Object.keys(ownProperties).includes(field));
}

function isReactComponent(result) {
  if (!result || !result.preview) {
    return;
  }

  const ownProperties = result.preview.ownProperties;

  if (!ownProperties) {
    return;
  }

  return Object.keys(ownProperties).includes("_reactInternalInstance") || Object.keys(ownProperties).includes("_reactInternalFiber");
}

function isConsole(expression) {
  return /^console/.test(expression);
}