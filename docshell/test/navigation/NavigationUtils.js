/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// /////////////////////////////////////////////////////////////////////////
//
// Utilities for navigation tests
//
// /////////////////////////////////////////////////////////////////////////

var body = "This frame was navigated.";
var target_url = "navigation_target_url.html";

var popup_body = "This is a popup";
var target_popup_url = "navigation_target_popup_url.html";

// /////////////////////////////////////////////////////////////////////////
// Functions that navigate frames
// /////////////////////////////////////////////////////////////////////////

function navigateByLocation(wnd) {
  try {
    wnd.location = target_url;
  } catch (ex) {
    // We need to keep our finished frames count consistent.
    // Oddly, this ends up simulating the behavior of IE7.
    window.open(target_url, "_blank", "width=10,height=10");
  }
}

function navigateByOpen(name) {
  window.open(target_url, name, "width=10,height=10");
}

function navigateByForm(name) {
  var form = document.createElement("form");
  form.action = target_url;
  form.method = "POST";
  form.target = name; document.body.appendChild(form);
  form.submit();
}

var hyperlink_count = 0;

function navigateByHyperlink(name) {
  var link = document.createElement("a");
  link.href = target_url;
  link.target = name;
  link.id = "navigation_hyperlink_" + hyperlink_count++;
  document.body.appendChild(link);
  sendMouseEvent({type: "click"}, link.id);
}

// /////////////////////////////////////////////////////////////////////////
// Functions that call into Mochitest framework
// /////////////////////////////////////////////////////////////////////////

function isNavigated(wnd, message) {
  var result = null;
  try {
    result = SpecialPowers.wrap(wnd).document.body.innerHTML.trim();
  } catch (ex) {
    result = ex;
  }
  is(result, body, message);
}

function isBlank(wnd, message) {
  var result = null;
  try {
    result = wnd.document.body.innerHTML.trim();
  } catch (ex) {
    result = ex;
  }
  is(result, "This is a blank document.", message);
}

function isAccessible(wnd, message) {
  try {
    wnd.document.body.innerHTML;
    ok(true, message);
  } catch (ex) {
    ok(false, message);
  }
}

function isInaccessible(wnd, message) {
  try {
    wnd.document.body.innerHTML;
    ok(false, message);
  } catch (ex) {
    ok(true, message);
  }
}

// /////////////////////////////////////////////////////////////////////////
// Functions that require UniversalXPConnect privilege
// /////////////////////////////////////////////////////////////////////////
// Replacing the getService with Services.ww format causes test errors, so ignore for now
/* eslint-disable mozilla/use-services */
function xpcEnumerateContentWindows(callback) {
  var Ci = SpecialPowers.Ci;
  var ww = SpecialPowers.Cc["@mozilla.org/embedcomp/window-watcher;1"]
                        .getService(Ci.nsIWindowWatcher);

  var contentWindows = [];

  for (let win of ww.getWindowEnumerator()) {
    if (win.isChromeWindow) {
      var docshellTreeNode = win.docShell;
      var childCount = docshellTreeNode.childCount;
      for (var i = 0; i < childCount; ++i) {
        var childTreeNode = docshellTreeNode.getChildAt(i);

        // we're only interested in content docshells
        if (SpecialPowers.unwrap(childTreeNode.itemType) != Ci.nsIDocShellTreeItem.typeContent)
          continue;

        var webNav = childTreeNode.QueryInterface(Ci.nsIWebNavigation);
        contentWindows.push(webNav.document.defaultView);
      }
    } else {
      contentWindows.push(win);
    }
  }

  while (contentWindows.length > 0)
    callback(contentWindows.pop());
}
/* eslint-enable mozilla/use-services */

// Note: This only searches for top-level frames with this name.
function xpcGetFramesByName(name) {
  var results = [];

  xpcEnumerateContentWindows(function(win) {
    if (win.name == name)
      results.push(win);
  });

  return results;
}

function xpcCleanupWindows() {
  xpcEnumerateContentWindows(function(win) {
    if (win.location &&
        (win.location.href.endsWith(target_url) ||
         win.location.href.endsWith(target_popup_url))) {
      win.close();
    }
  });
}

function xpcWaitForFinishedFrames(callback, numFrames) {
  var finishedFrameCount = 0;
  function frameFinished() {
    finishedFrameCount++;

    if (finishedFrameCount == numFrames) {
      clearInterval(frameWaitInterval);
      setTimeout(callback, 0);
      return;
    }

    if (finishedFrameCount > numFrames)
      throw new Error("Too many frames loaded.");
  }

  var finishedWindows = [];

  function contains(obj, arr) {
    for (var i = 0; i < arr.length; i++) {
      if (obj === arr[i])
        return true;
    }
    return false;
  }

  function searchForFinishedFrames(win) {
    if ((win.location.href.endsWith(target_url) ||
         win.location.href.endsWith(target_popup_url)) &&
        win.document &&
        win.document.body &&
        (win.document.body.textContent.trim() == body ||
         win.document.body.textContent.trim() == popup_body) &&
        win.document.readyState == "complete") {
      var windowId = win.windowUtils.outerWindowID;
      if (!contains(windowId, finishedWindows)) {
        finishedWindows.push(windowId);
        frameFinished();
      }
    }
    for (var i = 0; i < win.frames.length; i++)
      searchForFinishedFrames(win.frames[i]);
  }

  function poll() {
    try {
      // This only gives us UniversalXPConnect for the current stack frame
      // We're using setInterval, so the main page's privileges are still normal
      xpcEnumerateContentWindows(searchForFinishedFrames);
    } catch (ex) {
      // We might be accessing windows before they are fully constructed,
      // which can throw.  We'll find those frames on our next poll().
    }
  }

  var frameWaitInterval = setInterval(poll, 500);
}
