/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Loader } = require('sdk/test/loader');

exports.testOnClick = function (test) {
  let [loader, mockAlertServ] = makeLoader(module);
  let notifs = loader.require("sdk/notifications");
  let data = "test data";
  let opts = {
    onClick: function (clickedData) {
      test.assertEqual(this, notifs, "|this| should be notifications module");
      test.assertEqual(clickedData, data,
                       "data passed to onClick should be correct");
    },
    data: data,
    title: "test title",
    text: "test text",
    iconURL: "test icon URL"
  };
  notifs.notify(opts);
  mockAlertServ.click();
  loader.unload();
};

// Returns [loader, mockAlertService].
function makeLoader(test) {
  let loader = Loader(module);
  let mockAlertServ = {
    showAlertNotification: function (imageUrl, title, text, textClickable,
                                     cookie, alertListener, name) {
      this._cookie = cookie;
      this._alertListener = alertListener;
    },
    click: function () {
      this._alertListener.observe(null, "alertclickcallback", this._cookie);
    }
  };
  loader.require("sdk/notifications");
  let scope = loader.sandbox("sdk/notifications");
  scope.notify = mockAlertServ.showAlertNotification.bind(mockAlertServ);
  return [loader, mockAlertServ];
};
