/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Loader } = require('sdk/test/loader');

exports.testOnClick = function (assert) {
  let [loader, mockAlertServ] = makeLoader(module);
  let notifs = loader.require("sdk/notifications");
  let data = "test data";
  let opts = {
    onClick: function (clickedData) {
      assert.equal(this, notifs, "|this| should be notifications module");
      assert.equal(clickedData, data,
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
function makeLoader(module) {
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
}

require('sdk/test').run(exports);
