/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm", {});

/**
 * A function that can be used as part of a require hook for a
 * loader.js Loader.
 * This function handles "raw!" and "theme-loader!" requires.
 * See also: https://github.com/webpack/raw-loader.
 */
this.requireRawId = function(id, require) {
  const index = id.indexOf("!");
  const rawId = id.slice(index + 1);
  let uri = require.resolve(rawId);
  // If the original string did not end with ".js", then
  // require.resolve might have added the suffix.  We don't want to
  // add a suffix for a raw load (if needed the caller can specify it
  // manually), so remove it here.
  if (!id.endsWith(".js") && uri.endsWith(".js")) {
    uri = uri.slice(0, -3);
  }

  const stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true
  }).open2();

  const count = stream.available();
  const data = NetUtil.readInputStreamToString(stream, count, {
    charset: "UTF-8"
  });
  stream.close();

  // For the time being it doesn't seem worthwhile to cache the
  // result here.
  return data;
};

this.EXPORTED_SYMBOLS = ["requireRawId"];
