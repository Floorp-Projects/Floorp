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

function ok(condition, msg) {
  opener.ok(condition, "[rollback] " + msg);
  if (!condition) {
    opener.finish();
  }
}

// On Linux we sometimes receive fullscreenchange events before the window
// has finished transitioning to fullscreen. This can cause problems and
// test failures, so work around it on Linux until we can get a proper fix.
const workAroundFullscreenTransition = navigator.userAgent.indexOf("Linux") != -1;

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
  var listener = null;
  if (type === "enter") {
    // when entering fullscreen, ensure we don't call 'callback' until the
    // enter transition is complete.
    listener = function(event) {
      doc.removeEventListener("mozfullscreenchange", listener, false);
      if (!workAroundFullscreenTransition) {
        callback(event);
        return;
      }
      if (!inFullscreenMode()) {
        opener.todo(false, "fullscreenchange before entering fullscreen complete! " +
                    " window.fullScreen=" + window.fullScreen +
                    " normal=(" + normalSize.w + "," + normalSize.h + ")" +
                    " outerWidth=" + window.outerWidth + " width=" + window.screen.width +
                    " outerHeight=" + window.outerHeight + " height=" + window.screen.height);
        setTimeout(function(){listener(event);}, 100);
        return;
      }
      setTimeout(function(){callback(event)}, 0);
    };
  } else if (type === "exit") {
    listener = function(event) {
      doc.removeEventListener("mozfullscreenchange", listener, false);
      if (!workAroundFullscreenTransition) {
        callback(event);
        return;
      }
      if (!document.mozFullScreenElement && !inNormalMode()) {
        opener.todo(false, "fullscreenchange before exiting fullscreen complete! " +
                    " window.fullScreen=" + window.fullScreen +
                    " normal=(" + normalSize.w + "," + normalSize.h + ")" +
                    " outerWidth=" + window.outerWidth + " width=" + window.screen.width +
                    " outerHeight=" + window.outerHeight + " height=" + window.screen.height);
        // 'document' (*not* 'doc') has no fullscreen element, so we're trying
        // to completely exit fullscreen mode. Wait until the transition
        // to normal mode is complete before calling callback.
        setTimeout(function(){listener(event);}, 100);
        return;
      }
      opener.info("[rollback] Exited fullscreen");
      setTimeout(function(){callback(event);}, 0);
    };
  } else {
    throw "'type' must be either 'enter', or 'exit'.";
  }
  doc.addEventListener("mozfullscreenchange", listener, false);
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

