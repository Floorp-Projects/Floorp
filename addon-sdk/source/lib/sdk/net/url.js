/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cu, components } = require("chrome");
const { defer } = require("../core/promise");
const { merge } = require("../util/object");

const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});

/**
 * Open a channel synchronously for the URI given, with an optional charset, and
 * returns a resolved promise if succeed; rejected promise otherwise.
 */
function readSync(uri, charset) {
  let { promise, resolve, reject } = defer();

  try {
    resolve(readURISync(uri, charset));
  }
  catch (e) {
    reject("Failed to read: '" + uri + "' (Error Code: " + e.result + ")");
  }

  return promise;
}

/**
 * Open a channel synchronously for the URI given, with an optional charset, and
 * returns a promise.
 */
function readAsync(uri, charset) {
  let channel = NetUtil.newChannel(uri, charset, null);

  let { promise, resolve, reject } = defer();

  try {
    NetUtil.asyncFetch(channel, function (stream, result) {
      if (components.isSuccessCode(result)) {
        let count = stream.available();
        let data = NetUtil.readInputStreamToString(stream, count, { charset : charset });

        resolve(data);
      } else {
        reject("Failed to read: '" + uri + "' (Error Code: " + result + ")");
      }
    });
  }
  catch (e) {
    reject("Failed to read: '" + uri + "' (Error: " + e.message + ")");
  }

  return promise;
}

/**
 * Reads a URI and returns a promise. If the `sync` option is set to `true`, the
 * promise will be resolved synchronously.
 *
 * @param uri {string} The URI to read
 * @param [options] {object} This parameter can have any or all of the following
 * fields: `sync`, `charset`. By default the `charset` is set to 'UTF-8'.
 *
 * @returns {promise}  The promise that will be resolved with the content of the
 *          URL given.
  *
 * @example
 *  let promise = readURI('resource://gre/modules/NetUtil.jsm', {
 *    sync: true,
 *    charset: 'US-ASCII'
 });
 */
function readURI(uri, options) {
  options = merge({
    charset: "UTF-8",
    sync: false
  }, options);

  return options.sync
    ? readSync(uri, options.charset)
    : readAsync(uri, options.charset);
}

exports.readURI = readURI;

/**
 * Reads a URI synchronously.
 * This function is intentionally undocumented to favorites the `readURI` usage.
 *
 * @param uri {string} The URI to read
 * @param [charset] {string} The character set to use when read the content of
 *        the `uri` given.  By default is set to 'UTF-8'.
 *
 * @returns {string} The content of the URI given.
 *
 * @example
 *  let data = readURISync('resource://gre/modules/NetUtil.jsm');
 */
function readURISync(uri, charset) {
  charset = typeof charset === "string" ? charset : "UTF-8";

  let channel = NetUtil.newChannel(uri, charset, null);
  let stream = channel.open();

  let count = stream.available();
  let data = NetUtil.readInputStreamToString(stream, count, { charset : charset });

  stream.close();

  return data;
}

exports.readURISync = readURISync;
