const ALLOW_ACTION = SpecialPowers.Ci.nsIPermissionManager.ALLOW_ACTION;
const DENY_ACTION = SpecialPowers.Ci.nsIPermissionManager.DENY_ACTION;
const UNKNOWN_ACTION = SpecialPowers.Ci.nsIPermissionManager.UNKNOWN_ACTION;
const PROMPT_ACTION = SpecialPowers.Ci.nsIPermissionManager.PROMPT_ACTION;

/**
 * Dispatches |handler| to |element|, as if fired in response to |event|.
 */
function send(element, event, handler) {
  function unique_handler() { return handler.apply(this, arguments) }
  element.addEventListener(event, unique_handler, false);
  try { sendMouseEvent({ type: event }, element.id) }
  finally { element.removeEventListener(event, unique_handler, false) }
}

/**
 * Because it's not nice to leave popup windows open after the tests are
 * finished, we need a foolproof way to close some/all window.opened windows.
 */
(function(originalOpen) {
  var wins = [];
  (window.open = function() {
    var win = originalOpen.apply(window, arguments);
    if (win)
      wins[wins.length] = win;
    return win;
  }).close = function(n) {
    var promises = [];
    if (arguments.length < 1)
      n = wins.length;
    while (n --> 0) {
      var win = wins.pop();
      if (win) {
        promises.push((function(openedWindow) {
          return new Promise(function(resolve) {
            SpecialPowers.addObserver(function observer(subject, topic, data) {
              if (subject == openedWindow) {
                SpecialPowers.removeObserver(observer, "dom-window-destroyed");
                SimpleTest.executeSoon(resolve);
              }
            }, "dom-window-destroyed", false);
          });
        })(win));
        win.close();
      } else {
        promises.push(Promise.resolve());
        break;
      }
    }
    return Promise.all(promises);
  };
})(window.open);
