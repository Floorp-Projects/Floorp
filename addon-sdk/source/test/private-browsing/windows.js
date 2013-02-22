/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { pb, pbUtils } = require('./helper');
const { openDialog } = require('sdk/window/utils');
const { isPrivate } = require('sdk/private-browsing');
const { browserWindows: windows } = require('sdk/windows');

exports.testPerWindowPrivateBrowsingGetter = function(assert, done) {
  let win = openDialog({
    private: true
  });

  win.addEventListener('DOMContentLoaded', function onload() {
    win.removeEventListener('DOMContentLoaded', onload, false);

    assert.equal(pbUtils.getMode(win),
                 true, 'Newly opened window is in PB mode');
    assert.equal(pb.isActive, false, 'PB mode is not active');

    win.addEventListener("unload", function onunload() {
      win.removeEventListener('unload', onload, false);
      assert.equal(pb.isActive, false, 'PB mode is not active');
      done();
    }, false);

    win.close();
  }, false);
}

exports.testIsPrivateOnWindowOn = function(assert, done) {
  windows.open({
    isPrivate: true,
    onOpen: function(window) {
      assert.equal(isPrivate(window), false, 'isPrivate for a window is true when it should be');
      assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
      window.close(done);
    }
  });
}

exports.testIsPrivateOnWindowOff = function(assert, done) {
  windows.open({
    onOpen: function(window) {
      assert.equal(isPrivate(window), false, 'isPrivate for a window is false when it should be');
      assert.equal(isPrivate(window.tabs[0]), false, 'isPrivate for a tab is false when it should be');
      window.close(done);
    }
  })
}

require("test").run(exports);
