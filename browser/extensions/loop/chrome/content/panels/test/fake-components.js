"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this
               * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () {
  "use strict";

  // Create fake Components to test chrome-privileged React components.
  window.Components = { 
    utils: { 
      import: function _import() {
        return { 
          LoopAPI: { 
            sendMessageToHandler: function sendMessageToHandler(_ref, callback) {var name = _ref.name;
              switch (name) {
                case "GetLocale":
                  return callback("en-US");
                case "GetPluralRule":
                  return callback(1);
                default:
                  return callback();}} } };} } };})();
