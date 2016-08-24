/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu } = Components;
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});

/**
 * A function that can be used as part of a require hook for a
 * loader.js Loader.  This function only handles webpack-style "raw!"
 * requires; other requires should not be passed to this.  See
 * https://github.com/webpack/raw-loader.
 */
function requireRawId(id, require) {
  // Add the chrome:// protocol for properties files if missing (see Bug 1294220)
  if (id.endsWith(".properties") && !id.startsWith("raw!chrome://")) {
    id = id.replace("raw!", "raw!chrome://");
  }

  let uri = require.resolve(id.slice(4));
  // If the original string did not end with ".js", then
  // require.resolve might have added the suffix.  We don't want to
  // add a suffix for a raw load (if needed the caller can specify it
  // manually), so remove it here.
  if (!id.endsWith(".js") && uri.endsWith(".js")) {
    uri = uri.slice(0, -3);
  }


  let stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true
  }).open2();

  let count = stream.available();
  let data = NetUtil.readInputStreamToString(stream, count, {
    charset: "UTF-8"
  });
  stream.close();

  // For the time being it doesn't seem worthwhile to cache the
  // result here.
  return data;
}

this.EXPORTED_SYMBOLS = ["requireRawId"];
