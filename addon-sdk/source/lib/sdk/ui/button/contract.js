/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { contract } = require('../../util/contract');
const { isLocalURL } = require('../../url');
const { isNil, isObject, isString } = require('../../lang/type');
const { required, either, string, boolean, object } = require('../../deprecated/api-utils');
const { merge } = require('../../util/object');

function isIconSet(icons) {
  return Object.keys(icons).
    every(size => String(size >>> 0) === size && isLocalURL(icons[size]))
}

let iconSet = {
  is: either(object, string),
  ok: v => (isString(v) && isLocalURL(v)) || (isObject(v) && isIconSet(v)),
  msg: 'The option "icon" must be a local URL or an object with ' +
    'numeric keys / local URL values pair.'
}

let id = {
  is: string,
  ok: v => /^[a-z-_][a-z0-9-_]*$/i.test(v),
  msg: 'The option "id" must be a valid alphanumeric id (hyphens and ' +
        'underscores are allowed).'
};

let label = {
  is: string,
  ok: v => isNil(v) || v.trim().length > 0,
  msg: 'The option "label" must be a non empty string'
}

let stateContract = contract({
  label: label,
  icon: iconSet,
  disabled: boolean
});

exports.stateContract = stateContract;

let buttonContract = contract(merge({}, stateContract.rules, {
  id: required(id),
  label: required(label),
  icon: required(iconSet)
}));

exports.buttonContract = buttonContract;

exports.toggleStateContract = contract(merge({
  checked: boolean
}, stateContract.rules));

exports.toggleButtonContract = contract(merge({
  checked: boolean
}, buttonContract.rules));

