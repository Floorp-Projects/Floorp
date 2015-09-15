/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { setTimeout } = require('sdk/timers');
const tabs = require('sdk/tabs');

var closeEvents = 0;
const closeEventDetector = _ => closeEvents++;

exports.testNoTabCloseOnStartup = function(assert, done) {
  setTimeout(_ => {
    assert.equal(closeEvents, 0, 'there were no tab close events detected');
    tabs.open({
      url: 'about:mozilla',
      inBackground: true,
      onReady: tab => tab.close(),
      onClose: _ => {
        assert.equal(closeEvents, 1, 'there was one tab close event detected');
        done();
      }
    })
  });
}

exports.main = function() {
  tabs.on('close', closeEventDetector);

  require("sdk/test/runner").runTestsFromModule(module);
}
