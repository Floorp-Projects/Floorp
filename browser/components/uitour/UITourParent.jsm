/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["UITourParent"];

let { UITour } = ChromeUtils.import("resource:///modules/UITour.jsm");

class UITourParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "UITour:onPageEvent":
        if (this.manager.rootFrameLoader) {
          let browser = this.manager.rootFrameLoader.ownerElement;
          UITour.onPageEvent(message.data, browser);
          break;
        }
    }
  }
}
