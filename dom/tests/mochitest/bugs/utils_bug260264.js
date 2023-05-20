const ALLOW_ACTION = SpecialPowers.Ci.nsIPermissionManager.ALLOW_ACTION;
const DENY_ACTION = SpecialPowers.Ci.nsIPermissionManager.DENY_ACTION;
const UNKNOWN_ACTION = SpecialPowers.Ci.nsIPermissionManager.UNKNOWN_ACTION;
const PROMPT_ACTION = SpecialPowers.Ci.nsIPermissionManager.PROMPT_ACTION;

/**
 * Dispatches |handler| to |element|, as if fired in response to |event|.
 */
function send(element, event, handler) {
  function unique_handler() {
    return handler.apply(this, arguments);
  }
  element.addEventListener(event, unique_handler);
  try {
    sendMouseEvent({ type: event }, element.id);
  } finally {
    element.removeEventListener(event, unique_handler);
  }
}

/**
 * Because it's not nice to leave popup windows open after the tests are
 * finished, we need a foolproof way to close some/all window.opened windows.
 */
(function (originalOpen) {
  var wins = [];
  (window.open = function () {
    var win = originalOpen.apply(window, arguments);
    if (win) {
      wins[wins.length] = win;
    }
    return win;
  }).close = function (n) {
    var promises = [];
    if (arguments.length < 1) {
      n = wins.length;
    }
    while (n-- > 0) {
      var win = wins.pop();
      if (win) {
        let openedBrowsingContextID = SpecialPowers.getBrowsingContextID(win);
        promises.push(
          (function (openedWindow) {
            return new Promise(function (resolve) {
              let observer = {
                observe(subject) {
                  if (subject.id == openedBrowsingContextID) {
                    SpecialPowers.removeObserver(
                      observer,
                      "browsing-context-discarded"
                    );
                    SimpleTest.executeSoon(resolve);
                  }
                },
              };

              SpecialPowers.addObserver(observer, "browsing-context-discarded");
            });
          })(win)
        );
        win.close();
      } else {
        promises.push(Promise.resolve());
        break;
      }
    }
    return Promise.all(promises);
  };
})(window.open);
