/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  DeveloperToolbarTest.test("about:blank", function GAT_test() {
    DeveloperToolbarTest.checkInputStatus({
      typed: "resize toggle",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isOpen(), "responsive mode is open");

    DeveloperToolbarTest.checkInputStatus({
      typed: "resize toggle",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isClosed(), "responsive mode is closed");

    DeveloperToolbarTest.checkInputStatus({
      typed: "resize on",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isOpen(), "responsive mode is open");

    DeveloperToolbarTest.checkInputStatus({
      typed: "resize off",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isClosed(), "responsive mode is closed");

    DeveloperToolbarTest.checkInputStatus({
      typed: "resize to 400 400",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isOpen(), "responsive mode is open");

    DeveloperToolbarTest.checkInputStatus({
      typed: "resize off",
      status: "VALID"
    });
    DeveloperToolbarTest.exec();
    ok(isClosed(), "responsive mode is closed");

    executeSoon(finish);
  });

  function isOpen() {
    return !!gBrowser.selectedTab.__responsiveUI;
  }

  function isClosed() {
    return !isOpen();
  }
}
