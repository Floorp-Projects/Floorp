/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test added with bug 460086 to test the behavior of the new API that was added
 * to remove all traces of visiting a site.
 */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  PRIVATEBROWSING_CONTRACT_ID = "@mozilla.org/privatebrowsing-wrapper;1";
  load("do_test_removeDataFromDomain.js");
  do_test();

  // Shutdown the download manager.
  Services.obs.notifyObservers(null, "quit-application", null);
}
