// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function onLoad() {
  if (window.arguments[0].disconnectAccount) {
    document.getElementById("disconnectSync").hidden = true;
  } else {
    document.getElementById("disconnectAcct").hidden = true;
  }
  document.addEventListener(
    "dialogaccept",
    () => (window.arguments[0].confirmed = true)
  );
}
