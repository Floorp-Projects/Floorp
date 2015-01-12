/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { uri } = module;
const prefix = uri.substr(0, uri.lastIndexOf("/") + 1) + "fixtures/";

exports.url = (path="") => path && path.contains(":")
  ? path 
  : prefix + path.replace(/^\.\//, "");
