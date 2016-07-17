/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { asyncWindowLeakTest } = require("./leak-utils");
const { Loader } = require('sdk/test/loader');
const openWindow = require("sdk/window/utils").open;

exports["test sdk/event/dom does not leak when attached to closed window"] = function*(assert) {
  yield asyncWindowLeakTest(assert, _ => {
    return new Promise(resolve => {
      let loader = Loader(module);
      let { open } = loader.require('sdk/event/dom');
      let w = openWindow();
      w.addEventListener("DOMWindowClose", function windowClosed(evt) {
        w.removeEventListener("DOMWindowClose", windowClosed);
        // The sdk/event/dom module tries to clean itself up when DOMWindowClose
        // is fired.  Verify that it doesn't leak if its attached to an
        // already closed window either. (See bug 1268898.)
        open(w.document, "TestEvent1");
        resolve(loader);
      });
      w.close();
    });
  });
}

require("sdk/test").run(exports);
