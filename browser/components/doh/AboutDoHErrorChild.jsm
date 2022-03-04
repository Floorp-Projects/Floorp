/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutDoHErrorChild"];

class AboutDoHErrorChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "DoHAllowFallback": {
        this.sendAsyncMessage("DoH:AllowFallback");
        break;
      }
      case "DoHRetry": {
        this.sendAsyncMessage("DoH:Retry");
        break;
      }
    }
  }
}
