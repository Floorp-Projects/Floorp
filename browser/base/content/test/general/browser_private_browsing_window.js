// Make sure that we can open private browsing windows

function test() {
  waitForExplicitFinish();
  var nonPrivateWin = OpenBrowserWindow();
  ok(
    !PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin),
    "OpenBrowserWindow() should open a normal window"
  );
  nonPrivateWin.close();

  var privateWin = OpenBrowserWindow({ private: true });
  ok(
    PrivateBrowsingUtils.isWindowPrivate(privateWin),
    "OpenBrowserWindow({private: true}) should open a private window"
  );

  nonPrivateWin = OpenBrowserWindow({ private: false });
  ok(
    !PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin),
    "OpenBrowserWindow({private: false}) should open a normal window"
  );
  nonPrivateWin.close();

  whenDelayedStartupFinished(privateWin, function() {
    nonPrivateWin = privateWin.OpenBrowserWindow({ private: false });
    ok(
      !PrivateBrowsingUtils.isWindowPrivate(nonPrivateWin),
      "privateWin.OpenBrowserWindow({private: false}) should open a normal window"
    );

    nonPrivateWin.close();

    [
      {
        normal: "menu_newNavigator",
        private: "menu_newPrivateWindow",
        accesskey: true,
      },
      {
        normal: "appmenu_newNavigator",
        private: "appmenu_newPrivateWindow",
        accesskey: false,
      },
    ].forEach(function(menu) {
      let newWindow = privateWin.document.getElementById(menu.normal);
      let newPrivateWindow = privateWin.document.getElementById(menu.private);
      if (newWindow && newPrivateWindow) {
        ok(
          !newPrivateWindow.hidden,
          "New Private Window menu item should be hidden"
        );
        isnot(
          newWindow.label,
          newPrivateWindow.label,
          "New Window's label shouldn't be overwritten"
        );
        if (menu.accesskey) {
          isnot(
            newWindow.accessKey,
            newPrivateWindow.accessKey,
            "New Window's accessKey shouldn't be overwritten"
          );
        }
        isnot(
          newWindow.command,
          newPrivateWindow.command,
          "New Window's command shouldn't be overwritten"
        );
      }
    });

    is(
      privateWin.gBrowser.tabs[0].label,
      "New Private Tab",
      "New tabs in the private browsing windows should have 'New Private Tab' as the title."
    );

    privateWin.close();

    Services.prefs.setBoolPref("browser.privatebrowsing.autostart", true);
    privateWin = OpenBrowserWindow({ private: true });
    whenDelayedStartupFinished(privateWin, function() {
      [
        {
          normal: "menu_newNavigator",
          private: "menu_newPrivateWindow",
          accessKey: true,
        },
        {
          normal: "appmenu_newNavigator",
          private: "appmenu_newPrivateWindow",
          accessKey: false,
        },
      ].forEach(function(menu) {
        let newWindow = privateWin.document.getElementById(menu.normal);
        let newPrivateWindow = privateWin.document.getElementById(menu.private);
        if (newWindow && newPrivateWindow) {
          ok(
            newPrivateWindow.hidden,
            "New Private Window menu item should be hidden"
          );
          is(
            newWindow.label,
            newPrivateWindow.label,
            "New Window's label should be overwritten"
          );
          if (menu.accesskey) {
            is(
              newWindow.accessKey,
              newPrivateWindow.accessKey,
              "New Window's accessKey should be overwritten"
            );
          }
          is(
            newWindow.command,
            newPrivateWindow.command,
            "New Window's command should be overwritten"
          );
        }
      });

      is(
        privateWin.gBrowser.tabs[0].label,
        "New Tab",
        "Normal tab title is used also in the permanent private browsing mode."
      );
      privateWin.close();
      Services.prefs.clearUserPref("browser.privatebrowsing.autostart");
      finish();
    });
  });
}
