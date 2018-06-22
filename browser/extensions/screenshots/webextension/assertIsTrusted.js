/** For use with addEventListener, assures that any events have event.isTrusted set to true
      https://developer.mozilla.org/en-US/docs/Web/API/Event/isTrusted
    Should be applied *inside* catcher.watchFunction
*/
this.assertIsTrusted = function assertIsTrusted(handlerFunction) {
  return function(event) {
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
