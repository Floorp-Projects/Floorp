/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const {Cc, Ci} = require("chrome");
const ioService = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
const resourceHandler = ioService.getProtocolHandler("resource").
                        QueryInterface(Ci.nsIResProtocolHandler);

const URI = (uri, base=null) =>
  ioService.newURI(uri, null, base && URI(base))

const mount = (domain, uri) =>
  resourceHandler.setSubstitution(domain, ioService.newURI(uri, null, null));
exports.mount = mount;

const unmount = (domain, uri) =>
  resourceHandler.setSubstitution(domain, null);
exports.unmount = unmount;

const domain = 1;
const path = 2;
const resolve = (uri) => {
  const match = /resource\:\/\/([^\/]+)\/{0,1}([\s\S]*)/.exec(uri);
  const domain = match && match[1];
  const path = match && match[2];
  return !match ? null :
         !resourceHandler.hasSubstitution(domain) ? null :
  resourceHandler.resolveURI(URI(`/${path}`, `resource://${domain}/`));
}
exports.resolve = resolve;
