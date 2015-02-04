/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var count = 0;

setTimeout(function() {
  window.addEventListener("message", function(msg) {
    if (++count > 1) {
    	self.postMessage(msg.data);
    }
    else msg.source.postMessage(msg.data, '*');
  });

  document.getElementById('inner').src = iframePath;
}, 0);
