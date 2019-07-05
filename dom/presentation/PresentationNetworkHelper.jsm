// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { EventDispatcher } = ChromeUtils.import(
  "resource://gre/modules/Messaging.jsm"
);

function PresentationNetworkHelper() {}

PresentationNetworkHelper.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPresentationNetworkHelper]),

  getWifiIPAddress(aListener) {
    EventDispatcher.instance
      .sendRequestForResult({ type: "Wifi:GetIPAddress" })
      .then(
        result => aListener.onGetWifiIPAddress(result),
        err => aListener.onError(err)
      );
  },
};

var EXPORTED_SYMBOLS = ["PresentationNetworkHelper"];
