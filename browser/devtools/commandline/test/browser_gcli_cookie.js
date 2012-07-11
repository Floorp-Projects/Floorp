/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cookie commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,gcli-cookie";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    testCookieCommands();
    finish();
  });
}

function testCookieCommands() {
  DeveloperToolbarTest.checkInputStatus({
    typed: "cook",
    directTabText: "ie",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "cookie l",
    directTabText: "ist",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "cookie list",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "cookie remove",
    status: "ERROR",
    emptyParameters: [ " <key>" ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "cookie set",
    status: "ERROR",
    emptyParameters: [ " <key>", " <value>" ],
  });

  DeveloperToolbarTest.exec({
    typed: "cookie set fruit banana",
    args: {
      key: "fruit",
      value: "banana",
      path: "/",
      domain: null,
      secure: false
    },
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "cookie list",
    outputMatch: /Key/
  });

  DeveloperToolbarTest.exec({
    typed: "cookie remove fruit",
    args: { key: "fruit" },
    blankOutput: true,
  });
}
