/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

const {
  getMockedModule,
  setMockedModule,
  removeMockedModule,
} = require("devtools/shared/loader/browser-loader-mocks");
const { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window,
});

// Check that modules can be mocked in the browser loader.
// Test with a custom test module under the chrome:// scheme.
function testWithChromeScheme() {
  // Full chrome URI for the test module.
  const CHROME_URI = CHROME_URL_ROOT + "test-mocked-module.js";
  const ORIGINAL_VALUE = "Original value";
  const MOCKED_VALUE_1 = "Mocked value 1";
  const MOCKED_VALUE_2 = "Mocked value 2";

  const m1 = browserRequire(CHROME_URI);
  ok(m1, "Regular module can be required");
  is(m1.methodToMock(), ORIGINAL_VALUE, "Method returns the expected value");
  is(m1.someProperty, "someProperty", "Property has the expected value");

  info("Create a simple mocked version of the test module");
  const mockedModule = {
    methodToMock: () => MOCKED_VALUE_1,
    someProperty: "somePropertyMocked",
  };
  setMockedModule(mockedModule, CHROME_URI);
  ok(!!getMockedModule(CHROME_URI), "Has an entry for the chrome URI.");

  const m2 = browserRequire(CHROME_URI);
  ok(m2, "Mocked module can be required via chrome URI");
  is(
    m2.methodToMock(),
    MOCKED_VALUE_1,
    "Mocked method returns the expected value"
  );
  is(
    m2.someProperty,
    "somePropertyMocked",
    "Mocked property has the expected value"
  );

  const { methodToMock: requiredMethod } = browserRequire(CHROME_URI);
  ok(
    requiredMethod() === MOCKED_VALUE_1,
    "Mocked method returns the expected value when imported with destructuring"
  );

  info("Update the mocked method to return a different value");
  mockedModule.methodToMock = () => MOCKED_VALUE_2;
  is(
    requiredMethod(),
    MOCKED_VALUE_2,
    "Mocked method returns the updated value  when imported with destructuring"
  );

  info("Remove the mock for the test module");
  removeMockedModule(CHROME_URI);
  ok(!getMockedModule(CHROME_URI), "Has no entry for the chrome URI.");

  const m3 = browserRequire(CHROME_URI);
  ok(m3, "Regular module can be required after removing the mock");
  is(
    m3.methodToMock(),
    ORIGINAL_VALUE,
    "Method on module returns the expected value"
  );
}

// Similar tests as in testWithChromeScheme, but this time with a devtools module
// available under the resource:// scheme.
function testWithRegularDevtoolsModule() {
  // Testing with devtools/shared/path because it is a simple module, that can be imported
  // with destructuring. Any other module would do.
  const DEVTOOLS_MODULE_PATH = "devtools/shared/path";
  const DEVTOOLS_MODULE_URI = "resource://devtools/shared/path.js";

  const m1 = browserRequire(DEVTOOLS_MODULE_PATH);
  is(
    m1.joinURI("https://a", "b"),
    "https://a/b",
    "Original module was required"
  );

  info(
    "Set a mock for a sub-part of the path, which should not match require calls"
  );
  setMockedModule({ joinURI: () => "WRONG_PATH" }, "shared/path");

  ok(
    !getMockedModule(DEVTOOLS_MODULE_URI),
    "Has no mock entry for the full URI"
  );
  const m2 = browserRequire(DEVTOOLS_MODULE_PATH);
  is(
    m2.joinURI("https://a", "b"),
    "https://a/b",
    "Original module is still required"
  );

  info(
    "Set a mock for the complete path, which should now match require calls"
  );
  const mockedModule = {
    joinURI: () => "MOCKED VALUE",
  };
  setMockedModule(mockedModule, DEVTOOLS_MODULE_PATH);
  ok(
    !!getMockedModule(DEVTOOLS_MODULE_URI),
    "Has a mock entry for the full URI."
  );

  const m3 = browserRequire(DEVTOOLS_MODULE_PATH);
  is(
    m3.joinURI("https://a", "b"),
    "MOCKED VALUE",
    "The mocked module has been returned"
  );

  info(
    "Check that the mocked methods can be updated after a destructuring import"
  );
  const { joinURI } = browserRequire(DEVTOOLS_MODULE_PATH);
  mockedModule.joinURI = () => "UPDATED VALUE";
  is(
    joinURI("https://a", "b"),
    "UPDATED VALUE",
    "Mocked method was correctly updated"
  );

  removeMockedModule(DEVTOOLS_MODULE_PATH);
  ok(
    !getMockedModule(DEVTOOLS_MODULE_URI),
    "Has no mock entry for the full URI"
  );
  const m4 = browserRequire(DEVTOOLS_MODULE_PATH);
  is(
    m4.joinURI("https://a", "b"),
    "https://a/b",
    "Original module can be required again"
  );
}

function test() {
  testWithChromeScheme();
  testWithRegularDevtoolsModule();
  delete window.getBrowserLoaderForWindow;
  finish();
}
