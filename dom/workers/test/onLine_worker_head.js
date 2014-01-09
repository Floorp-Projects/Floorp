/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function info(text) {
  dump("Test for Bug 925437: worker: " + text + "\n");
}

function ok(test, message) {
  postMessage({ type: 'ok', test: test, message: message });
}

/**
 * Returns a handler function for an online/offline event. The returned handler
 * ensures the passed event object has expected properties and that the handler
 * is called at the right moment (according to the gState variable).
 * @param nameTemplate The string identifying the hanlder. '%1' in that
 *                     string will be replaced with the event name.
 * @param eventName 'online' or 'offline'
 * @param expectedState value of gState at the moment the handler is called.
 *                       The handler increases gState by one before checking
 *                       if it matches expectedState.
 */
function makeHandler(nameTemplate, eventName, expectedState, prefix, custom) {
  prefix += ": ";
  return function(e) {
    var name = nameTemplate.replace(/%1/, eventName);
    ok(e.constructor == Event, prefix + "event should be an Event");
    ok(e.type == eventName, prefix + "event type should be " + eventName);
    ok(!e.bubbles, prefix + "event should not bubble");
    ok(!e.cancelable, prefix + "event should not be cancelable");
    ok(e.target == self, prefix + "the event target should be the worker scope");
    ok(eventName == 'online' ? navigator.onLine : !navigator.onLine, prefix + "navigator.onLine " + navigator.onLine + " should reflect event " + eventName);

    if (custom) {
      custom();
    }
  }
}



