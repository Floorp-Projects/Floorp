"use strict";

/*
When run locally this won't test whether the files are packaged and available
in a distributed build unless `./mach mochitest --appname dist` is used
(after `./mach package`)
*/

function test() {
  waitForExplicitFinish();

  const windowListener = {
    onOpenWindow(win) {
      info("Observed window open");

      const domWindow = win.docShell.domWindow;
      waitForFocus(() => {
        is(
          domWindow.location.href,
          "chrome://layoutdebug/content/layoutdebug.xhtml",
          "Window location is correct"
        );
        domWindow.close();
      }, domWindow);
    },

    onCloseWindow() {
      info("Observed window closed");
      Services.wm.removeListener(this);
      finish();
    },
  };
  Services.wm.addListener(windowListener);

  const menuitem = document.getElementById("menu_layout_debugger");
  ok(menuitem, "Menuitem present");
  if (menuitem) {
    // open the debugger window
    menuitem.click();
  }
}
