/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {mount, unmount, resolve} = require("sdk/uri/resource");
const {tmpdir} = require("node/os");
const {fromFilename} = require("sdk/url");

const mountURI = fromFilename(tmpdir());

exports.testAPI = assert => {
  const domain = Math.random().toString(16).substr(2)

  assert.equal(resolve(`resource://${domain}`),
               null,
               "domain isn't mounted");

  mount(domain, mountURI);

  assert.equal(resolve(`resource://${domain}`),
               mountURI,
               "domain was mounted");


  assert.equal(resolve(`resource://${domain}/foo.js`),
               `${mountURI}foo.js`,
               "uri resolves to a file in mounted location");

  unmount(domain);

  assert.equal(resolve(`resource://${domain}`),
               null,
               "domain is no longer mounted");


  assert.equal(resolve(`resource://${domain}/foo.js`),
               null,
               "uris under unmounted domain resolve to null");

};

require("sdk/test").run(exports);
