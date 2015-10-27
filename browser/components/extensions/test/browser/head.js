var {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm");

function makeWidgetId(id)
{
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

function* focusWindow(win)
{
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  if (fm.activeWindow == win) {
    return;
  }

  let promise = new Promise(resolve => {
    win.addEventListener("focus", function listener() {
      win.removeEventListener("focus", listener, true);
      resolve();
    }, true);
  });

  win.focus();
  yield promise;
}
