/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { contract } = require('../../util/contract');
const { isValidURI, URL, isLocalURL } = require('../../url');
const { isNil, isObject, isString } = require('../../lang/type');

exports.contract = contract({
  id: {
  	is: [ 'string', 'undefined' ],
  	ok: v => /^[a-z0-9-_]+$/i.test(v),
    msg: 'The option "id" must be a valid alphanumeric id (hyphens and ' +
         'underscores are allowed).'
  },
  title: {
  	is: [ 'string' ],
  	ok: v => v.length
  },
  url: {
    is: [ 'string' ],
    ok: v => isLocalURL(v),
    map: v => v.toString(),
    msg: 'The option "url" must be a valid local URI.'
  }
});
