/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const tabs = require('sdk/tabs');
const { isPrivate } = require('sdk/private-browsing');
const pbUtils = require('sdk/private-browsing/utils');

exports.testPrivateTabsAreListed = function (assert, done) {
  let originalTabCount = tabs.length;

  tabs.open({
    url: 'about:blank',
    isPrivate: true,
    onOpen: function(tab) {
      // PWPB case
      if (pbUtils.isWindowPBSupported || pbUtils.isTabPBSupported) {
        assert.ok(isPrivate(tab), "tab is private");
        assert.equal(tabs.length, originalTabCount + 1,
                     'New private window\'s tab are visible in tabs list');
      }
      else {
      // Global case, openDialog didn't opened a private window/tab
        assert.ok(!isPrivate(tab), "tab isn't private");
        assert.equal(tabs.length, originalTabCount + 1,
                     'New non-private window\'s tab is visible in tabs list');
      }

      tab.close(done);
    }
  });
};

