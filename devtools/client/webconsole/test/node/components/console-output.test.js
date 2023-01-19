/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
// Test utils.
const expect = require("expect");
const { render } = require("enzyme");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);

const ConsoleOutput = createFactory(
  require("resource://devtools/client/webconsole/components/Output/ConsoleOutput.js")
);
const serviceContainer = require("resource://devtools/client/webconsole/test/node/fixtures/serviceContainer.js");
const {
  setupStore,
} = require("resource://devtools/client/webconsole/test/node/helpers.js");

const MESSAGES_NUMBER = 100;
function getDefaultProps() {
  const store = setupStore(
    Array.from({ length: MESSAGES_NUMBER })
      // Alternate message so we don't trigger the repeat mechanism.
      .map((_, i) => (i % 2 ? "console.log(null)" : "console.log(NaN)"))
  );

  return {
    store,
    serviceContainer,
  };
}

describe("ConsoleOutput component:", () => {
  it("Render every message", () => {
    const Services = require("resource://devtools/client/shared/test-helpers/jest-fixtures/Services.js");
    Services.prefs.setBoolPref("devtools.testing", true);

    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const rendered = render(
      Provider({ store: setupStore() }, ConsoleOutput(getDefaultProps()))
    );

    Services.prefs.setBoolPref("devtools.testing", false);
    const visibleMessages = JSON.parse(rendered.prop("data-visible-messages"));
    expect(visibleMessages.length).toBe(MESSAGES_NUMBER);
  });
});
