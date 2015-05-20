// Remember the window size in non-fullscreen mode.
var normalSize = new function() {
  this.w = window.outerWidth;
  this.h = window.outerHeight;
}();

// Returns true if the window occupies the entire screen.
// Note this only returns true once the transition from normal to
// fullscreen mode is complete.
function inFullscreenMode() {
  return window.outerWidth == window.screen.width &&
         window.outerHeight == window.screen.height;
}

// Returns true if the window is in normal mode, i.e. non fullscreen mode.
// Note this only returns true once the transition from fullscreen back to
// normal mode is complete.
function inNormalMode() {
  return window.outerWidth == normalSize.w &&
         window.outerHeight == normalSize.h;
}

// Adds a listener that will be called once a fullscreen transition
// is complete. When type==='enter', callback is called when we've
// received a fullscreenchange event, and the fullscreen transition is
// complete. When type==='exit', callback is called when we've
// received a fullscreenchange event and the window dimensions match
// the window dimensions when the window opened (so don't resize the
// window while running your test!). inDoc is the document which
// the listeners are added on, if absent, the listeners are added to
// the current document.
function addFullscreenChangeContinuation(type, callback, inDoc) {
  var doc = inDoc || document;
  function checkCondition() {
    if (type == "enter") {
      return inFullscreenMode();
    } else if (type == "exit") {
      // If we just revert the state to a previous fullscreen state,
      // the window won't back to the normal mode. Hence we check
      // mozFullScreenElement first here.
      return doc.mozFullScreenElement || inNormalMode();
    } else {
      throw "'type' must be either 'enter', or 'exit'.";
    }
  }
  function invokeCallback(event) {
    // Use async call after a paint to workaround unfinished fullscreen
    // change even when the window size has changed on Linux.
    requestAnimationFrame(() => setTimeout(() => callback(event), 0), 0);
  }
  function onFullscreenChange(event) {
    doc.removeEventListener("mozfullscreenchange", onFullscreenChange, false);
    if (checkCondition()) {
      invokeCallback(event);
      return;
    }
    var win = doc.defaultView;
    function onResize() {
      if (checkCondition()) {
        win.removeEventListener("resize", onResize, false);
        invokeCallback(event);
      }
    }
    win.addEventListener("resize", onResize, false);
  }
  doc.addEventListener("mozfullscreenchange", onFullscreenChange, false);
}

// Calls |callback| when the next fullscreenerror is dispatched to inDoc||document.
function addFullscreenErrorContinuation(callback, inDoc) {
  var doc = inDoc || document;
  var listener = function(event) {
    doc.removeEventListener("mozfullscreenerror", listener, false);
    setTimeout(function(){callback(event);}, 0);
  };
  doc.addEventListener("mozfullscreenerror", listener, false);
}

