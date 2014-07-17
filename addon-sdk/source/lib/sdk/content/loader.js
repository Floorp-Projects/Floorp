/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { EventEmitter } = require('../deprecated/events');
const { isValidURI, isLocalURL, URL } = require('../url');
const file = require('../io/file');
const { contract } = require('../util/contract');
const { isString, isNil, instanceOf } = require('../lang/type');
const { validateOptions,
  string, array, object, either, required } = require('../deprecated/api-utils');

const isJSONable = (value) => {
  try {
    JSON.parse(JSON.stringify(value));
  } catch (e) {
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

/**
 * Trait is intended to be used in some composition. It provides set of core
 * properties and bounded validations to them. Trait is useful for all the
 * compositions providing high level APIs for interaction with content.
 * Property changes emit `"propertyChange"` events on instances.
 */
const Loader = EventEmitter.compose({
  /**
   * Permissions for the content, with the following keys:
   * @property {Object} [allow = { script: true }]
   * @property {Boolean} [allow.script = true]
   *    Whether or not to execute script in the content.  Defaults to true.
   */
  get allow() this._allow || (this._allow = Allow(true)),
  set allow(value) this.allow.script = value && value.script,
  _allow: null,
  /**
   * The content to load. Either a string of HTML or a URL.
   * @type {String}
   */
  get contentURL() this._contentURL,
  set contentURL(value) {
    value = validate(value, valid.contentURL);
    if (this._contentURL != value) {
      this._emit('propertyChange', {
        contentURL: this._contentURL = value
      });
    }
  },
  _contentURL: null,
  /**
   * When to load the content scripts.
   * Possible values are "end" (default), which loads them once all page
   * contents have been loaded, "ready", which loads them once DOM nodes are
   * ready (ie like DOMContentLoaded event), and "start", which loads them once
   * the `window` object for the page has been created, but before any scripts
   * specified by the page have been loaded.
   * Property change emits `propertyChange` event on instance with this key
   * and new value.
   * @type {'start'|'ready'|'end'}
   */
  get contentScriptWhen() this._contentScriptWhen,
  set contentScriptWhen(value) {
    value = validate(value, valid.contentScriptWhen);
    if (value !== this._contentScriptWhen) {
      this._emit('propertyChange', {
        contentScriptWhen: this._contentScriptWhen = value
      });
    }
  },
  _contentScriptWhen: 'end',
  /**
   * Options avalaible from the content script as `self.options`.
   * The value of options can be of any type (object, array, string, etc.)
   * but only jsonable values will be available as frozen objects from the
   * content script.
   * Property change emits `propertyChange` event on instance with this key
   * and new value.
   * @type {Object}
   */
  get contentScriptOptions() this._contentScriptOptions,
  set contentScriptOptions(value) this._contentScriptOptions = value,
  _contentScriptOptions: null,
  /**
   * The URLs of content scripts.
   * Property change emits `propertyChange` event on instance with this key
   * and new value.
   * @type {String[]}
   */
  get contentScriptFile() this._contentScriptFile,
  set contentScriptFile(value) {
    value = validate(value, valid.contentScriptFile);
    if (value != this._contentScriptFile) {
      this._emit('propertyChange', {
        contentScriptFile: this._contentScriptFile = value
      });
    }
  },
  _contentScriptFile: null,
  /**
   * The texts of content script.
   * Property change emits `propertyChange` event on instance with this key
   * and new value.
   * @type {String|undefined}
   */
  get contentScript() this._contentScript,
  set contentScript(value) {
    value = validate(value, valid.contentScript);
    if (value != this._contentScript) {
      this._emit('propertyChange', {
        contentScript: this._contentScript = value
      });
    }
  },
  _contentScript: null
});
exports.Loader = Loader;

exports.contract = contract(valid);
