/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
// Test utils.
const expect = require("expect");
const { render } = require("enzyme");
const Provider = createFactory(require("react-redux").Provider);

const ConsoleOutput = createFactory(
  require("devtools/client/webconsole/components/Output/ConsoleOutput")
);
const serviceContainer = require("devtools/client/webconsole/test/node/fixtures/serviceContainer");
const { setupStore } = require("devtools/client/webconsole/test/node/helpers");
const { initialize } = require("devtools/client/webconsole/actions/ui");
const {
  getInitialMessageCountForViewport,
} = require("devtools/client/webconsole/utils/messages.js");

const MESSAGES_NUMBER = 100;
function getDefaultProps(initialized) {
  const store = setupStore(
    Array.from({ length: MESSAGES_NUMBER })
      // Alternate message so we don't trigger the repeat mechanism.
      .map((_, i) => (i % 2 ? "new Date(0)" : "console.log(NaN)"))
  );

  if (initialized) {
    store.dispatch(initialize());
  }

  return {
    store,
    serviceContainer,
  };
}

describe("ConsoleOutput component:", () => {
  it("Render only the last messages that fits the viewport when non-initialized", () => {
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const rendered = render(
      Provider({ store: setupStore() }, ConsoleOutput(getDefaultProps(false)))
    );
    const messagesNumber = rendered.find(".message").length;
    expect(messagesNumber).toBe(getInitialMessageCountForViewport(window));
  });

  it("Render every message when initialized", () => {
    // We need to wrap the ConsoleApiElement in a Provider in order for the
    // ObjectInspector to work.
    const rendered = render(
      Provider({ store: setupStore() }, ConsoleOutput(getDefaultProps(true)))
    );
    expect(rendered.find(".message").length).toBe(MESSAGES_NUMBER);
  });
});
