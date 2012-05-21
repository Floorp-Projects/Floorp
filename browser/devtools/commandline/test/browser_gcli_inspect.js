/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/test/browser_gcli_inspect.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    testInspect();

    finish();
  });
}

function testInspect() {
  DeveloperToolbarTest.checkInputStatus({
    typed: "inspec",
    directTabText: "t",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect",
    emptyParameters: [ " <node>" ],
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect h1",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect span",
    status: "ERROR"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect div",
    status: "VALID"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect .someclass",
    status: "VALID"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect #someid",
    status: "VALID"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect button[disabled]",
    status: "VALID"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect p>strong",
    status: "VALID"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed: "inspect :root",
    status: "VALID"
  });
}
