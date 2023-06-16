/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/** For use with addEventListener, assures that any events have event.isTrusted set to true
      https://developer.mozilla.org/en-US/docs/Web/API/Event/isTrusted
    Should be applied *inside* catcher.watchFunction
*/
this.assertIsTrusted = function assertIsTrusted(handlerFunction) {
  return function (event) {
    if (!event) {
      const exc = new Error("assertIsTrusted did not get an event");
      exc.noPopup = true;
      throw exc;
    }
    if (!event.isTrusted) {
      const exc = new Error(`Received untrusted event (type: ${event.type})`);
      exc.noPopup = true;
      throw exc;
    }
    return handlerFunction.call(this, event);
  };
};
null;
