/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Class } = require("../core/heritage");
lazyRequire(this, "../url", "isLocalURL");
lazyRequire(this, "./utils", "loadSheet", "removeSheet", "isTypeValid");
lazyRequire(this, "../lang/type", "isString");
const { attachTo, detachFrom } = require("../content/mod");
lazyRequire(this, '../self', "data");

const { freeze, create } = Object;

function Style({ source, uri, type }) {
  source = source == null ? null : freeze([].concat(source));
  uri = uri == null ? null : freeze([].concat(uri));
  type = type == null ? "author" : type;

  if (source && !source.every(isString))
    throw new Error('Style.source must be a string or an array of strings.');

  if (uri && !uri.every(isLocalURL))
    throw new Error('Style.uri must be a local URL or an array of local URLs');

  if (type && !isTypeValid(type))
    throw new Error('Style.type must be "agent", "user" or "author"');

  return freeze(create(Style.prototype, {
    "source": { value: source, enumerable: true },
    "uri": { value: uri, enumerable: true },
    "type": { value: type, enumerable: true }
  }));
};

exports.Style = Style;

attachTo.define(Style, function (style, window) {
  if (style.uri) {
    for (let uri of style.uri)
      loadSheet(window, data.url(uri), style.type);
  }

  if (style.source) {
    let uri = "data:text/css;charset=utf-8,";

    uri += encodeURIComponent(style.source.join(""));

    loadSheet(window, uri, style.type);
  }
});

detachFrom.define(Style, function (style, window) {
  if (style.uri)
    for (let uri of style.uri)
      removeSheet(window, data.url(uri));

  if (style.source) {
    let uri = "data:text/css;charset=utf-8,";

    uri += encodeURIComponent(style.source.join(""));

    removeSheet(window, uri, style.type);
  }
});
