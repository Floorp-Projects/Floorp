/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
On a return key, send the content of the textArea back to the add-on,
and zero the textArea for the next time.
*/

var textArea = document.getElementById('annotation-box');

textArea.onkeyup = function(event) {
  if (event.keyCode == 13) {
    self.postMessage(textArea.value);
    textArea.value = '';
  }
};

self.on('message', function() {
  var textArea = document.getElementById('annotation-box');
  textArea.value = '';
  textArea.focus();
});
