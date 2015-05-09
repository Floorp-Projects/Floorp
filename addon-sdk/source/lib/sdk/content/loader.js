/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { isValidURI, isLocalURL, URL } = require('../url');
const { contract } = require('../util/contract');
const { isString, isNil, instanceOf } = require('../lang/type');
const { validateOptions,
  string, array, object, either, required } = require('../deprecated/api-utils');

const isJSONable = (value) => {
  try {
    JSON.parse(JSON.stringify(value));
  }
  catch (e) {
    return false;
  }
  return true;
};

const isValidScriptFile = (value) =>
  (isString(value) || instanceOf(value, URL)) && isLocalURL(value);

// map of property validations
const valid = {
  contentURL: {
    is: either(string, object),
    ok: url => isNil(url) || isLocalURL(url) || isValidURI(url),
    msg: 'The `contentURL` option must be a valid URL.'
  },
  contentScriptFile: {
    is: either(string, object, array),
    ok: value => isNil(value) || [].concat(value).every(isValidScriptFile),
    msg: 'The `contentScriptFile` option must be a local URL or an array of URLs.'
  },
  contentScript: {
    is: either(string, array),
    ok: value => isNil(value) || [].concat(value).every(isString),
    msg: 'The `contentScript` option must be a string or an array of strings.'
  },
  contentScriptWhen: {
    is: required(string),
    map: value => value || 'end',
    ok: value => ~['start', 'ready', 'end'].indexOf(value),
    msg: 'The `contentScriptWhen` option must be either "start", "ready" or "end".'
  },
  contentScriptOptions: {
    ok: value => isNil(value) || isJSONable(value),
    msg: 'The contentScriptOptions should be a jsonable value.'
  }
};
exports.validationAttributes = valid;

/**
 * Shortcut function to validate property with validation.
 * @param {Object|Number|String} suspect
 *    value to validate
 * @param {Object} validation
 *    validation rule passed to `api-utils`
 */
function validate(suspect, validation) validateOptions(
  { $: suspect },
  { $: validation }
).$

function Allow(script) ({
  get script() script,
  set script(value) script = !!value
})

exports.contract = contract(valid);
