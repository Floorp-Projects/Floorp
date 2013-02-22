'use strict';

const { Ci } = require('chrome');
const { pb, pbUtils, loader: pbLoader, getOwnerWindow } = require('./helper');

exports.testIsPrivateOnTab = function(test) {
  const { openTab, closeTab } = pbLoader.require('sdk/tabs/utils');

  let window = pbLoader.require('sdk/windows').browserWindows.activeWindow;
  let chromeWindow = pbLoader.require('sdk/private-browsing/window/utils').getOwnerWindow(window);
  test.assert(chromeWindow instanceof Ci.nsIDOMWindow, 'associated window is found');
  test.assert(!pb.isPrivate(chromeWindow), 'the top level window is not private');

  let rawTab = openTab(chromeWindow, 'data:text/html,<h1>Hi!</h1>', {
  	isPrivate: true
  });

  // test that the tab is private
  test.assert(rawTab.browser.docShell.QueryInterface(Ci.nsILoadContext).usePrivateBrowsing);
  test.assert(pb.isPrivate(rawTab.browser.contentWindow));
  test.assert(pb.isPrivate(rawTab.browser));

  closeTab(rawTab);
}
