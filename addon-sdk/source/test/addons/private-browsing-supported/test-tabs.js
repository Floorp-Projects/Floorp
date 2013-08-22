'use strict';

const tabs = require('sdk/tabs');
const { isPrivate } = require('sdk/private-browsing');
const pbUtils = require('sdk/private-browsing/utils');
const { getOwnerWindow } = require('sdk/private-browsing/window/utils');

exports.testPrivateTabsAreListed = function (assert, done) {
  let originalTabCount = tabs.length;

  tabs.open({
    url: 'about:blank',
    isPrivate: true,
    onOpen: function(tab) {
      let win = getOwnerWindow(tab);
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

