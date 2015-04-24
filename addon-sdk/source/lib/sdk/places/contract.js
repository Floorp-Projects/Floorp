/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require('chrome');
const { isValidURI, URL } = require('../url');
const { contract } = require('../util/contract');
const { extend } = require('../util/object');

// map of property validations
const validItem = {
  id: {
    is: ['number', 'undefined', 'null'],
  },
  group: {
    is: ['object', 'number', 'undefined', 'null'],
    ok: function (value) {
      return value &&
        (value.toString && value.toString() === '[object Group]') ||
        typeof value === 'number' ||
        value.type === 'group';
    },
    msg: 'The `group` property must be a valid Group object'
  },
  index: {
    is: ['undefined', 'null', 'number'],
    map: function (value) value == null ? -1 : value,
    msg: 'The `index` property must be a number.'
  },
  updated: {
    is: ['number', 'undefined']
  }
};

const validTitle = {
  title: {
    is: ['string'],
    msg: 'The `title` property must be defined.'
  }
};

const validURL = {
  url: {
    is: ['string'],
    ok: isValidURI,
    msg: 'The `url` property must be a valid URL.'
  }
};

const validTags = {
  tags: {
    is: ['object'],
    ok: function (tags) tags instanceof Set,
    map: function (tags) {
      if (Array.isArray(tags))
        return new Set(tags);
      if (tags == null)
        return new Set();
      return tags;
    },
    msg: 'The `tags` property must be a Set, or an array'
  }
};

exports.bookmarkContract = contract(
  extend(validItem, validTitle, validURL, validTags));
exports.separatorContract = contract(validItem);
exports.groupContract = contract(extend(validItem, validTitle));
