/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class PointerLockChild extends JSWindowActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "MozDOMPointerLock:Entered":
        this.sendAsyncMessage("PointerLock:Entered");
        break;

      case "MozDOMPointerLock:Exited":
        this.sendAsyncMessage("PointerLock:Exited");
        break;
    }
  }
}
