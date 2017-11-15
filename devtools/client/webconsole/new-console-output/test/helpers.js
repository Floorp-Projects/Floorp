/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let ReactDOM = require("devtools/client/shared/vendor/react-dom");
let React = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { createElement } = React;
var TestUtils = React.addons.TestUtils;

const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const { configureStore } = require("devtools/client/webconsole/new-console-output/store");
const { IdGenerator } = require("devtools/client/webconsole/new-console-output/utils/id-generator");
const { stubPackets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stubs/index");
const {
  getAllMessagesById,
} = require("devtools/client/webconsole/new-console-output/selectors/messages");

/**
 * Prepare actions for use in testing.
 */
function setupActions() {
  // Some actions use dependency injection. This helps them avoid using state in
  // a hard-to-test way. We need to inject stubbed versions of these dependencies.
  const idGenerator = new IdGenerator();
  return Object.assign({}, actions, {
    messageAdd: packet => actions.messageAdd(packet, idGenerator),
    messagesAdd: packets => actions.messagesAdd(packets, idGenerator)
  });
}

/**
 * Prepare the store for use in testing.
 */
function setupStore(input = [], hud, options) {
  const store = configureStore(hud, options);

  // Add the messages from the input commands to the store.
  input.forEach((cmd) => {
    store.dispatch(actions.messageAdd(stubPackets.get(cmd)));
  });

  return store;
}

function renderComponent(component, props) {
  const el = createElement(component, props, {});
  // By default, renderIntoDocument() won't work for stateless components, but
  // it will work if the stateless component is wrapped in a stateful one.
  // See https://github.com/facebook/react/issues/4839
  const wrappedEl = dom.span({}, [el]);
  const renderedComponent = TestUtils.renderIntoDocument(wrappedEl);
  return ReactDOM.findDOMNode(renderedComponent).children[0];
}

function shallowRenderComponent(component, props) {
  const el = createElement(component, props);
  const renderer = TestUtils.createRenderer();
  renderer.render(el, {});
  return renderer.getRenderOutput();
}

/**
 * Create deep copy of given packet object.
 */
function clonePacket(packet) {
  return JSON.parse(JSON.stringify(packet));
}

/**
 * Return the message in the store at the given index.
 *
 * @param {object} state - The redux state of the console.
 * @param {int} index - The index of the message in the map.
 * @return {Message} - The message, or undefined if the index does not exists in the map.
 */
function getMessageAt(state, index) {
  const messages = getAllMessagesById(state);
  return messages.get([...messages.keys()][index]);
}

module.exports = {
  clonePacket,
  getMessageAt,
  renderComponent,
  setupActions,
  setupStore,
  shallowRenderComponent,
};
