/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { shallow } = require("enzyme");
const { REPS, getRep } = require("../rep");
const { PromiseRep } = REPS;
const { MODE } = require("../constants");
const stubs = require("../stubs/promise");
const {
  expectActorAttribute,
  getSelectableInInspectorGrips,
  getGripLengthBubbleText,
} = require("./test-helpers");

const renderRep = (object, props) => {
  return shallow(PromiseRep.rep({ object, ...props }));
};

describe("Promise - Pending", () => {
  const object = stubs.get("Pending");
  const defaultOutput = 'Promise { <state>: "pending" }';

  it("correctly selects PromiseRep Rep for pending Promise", () => {
    expect(getRep(object)).toBe(PromiseRep.rep);
  });

  it("renders as expected", () => {
    let component = renderRep(object, { mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep(object, { mode: MODE.TINY });
    expect(component.text()).toBe('Promise { "pending" }');
    expectActorAttribute(component, object.actor);

    component = renderRep(object, { mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep(object, { mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });
});

describe("Promise - fulfilled with string", () => {
  const object = stubs.get("FulfilledWithString");
  const defaultOutput = 'Promise { <state>: "fulfilled", <value>: "foo" }';

  it("selects PromiseRep Rep for Promise fulfilled with a string", () => {
    expect(getRep(object)).toBe(PromiseRep.rep);
  });

  it("should render as expected", () => {
    expect(renderRep(object, { mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep(object, { mode: MODE.TINY }).text()).toBe(
      'Promise { "fulfilled" }'
    );
    expect(renderRep(object, { mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep(object, { mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Promise - fulfilled with object", () => {
  const object = stubs.get("FulfilledWithObject");
  const defaultOutput = 'Promise { <state>: "fulfilled", <value>: {…} }';

  it("selects PromiseRep Rep for Promise fulfilled with an object", () => {
    expect(getRep(object)).toBe(PromiseRep.rep);
  });

  it("should render as expected", () => {
    expect(renderRep(object, { mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep(object, { mode: MODE.TINY }).text()).toBe(
      'Promise { "fulfilled" }'
    );
    expect(renderRep(object, { mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep(object, { mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Promise - fulfilled with array", () => {
  const object = stubs.get("FulfilledWithArray");
  const length = getGripLengthBubbleText(object.promiseState.value, {
    mode: MODE.TINY,
  });
  const out = `Promise { <state>: "fulfilled", <value>: ${length} […] }`;

  it("selects PromiseRep Rep for Promise fulfilled with an array", () => {
    expect(getRep(object)).toBe(PromiseRep.rep);
  });

  it("should render as expected", () => {
    expect(renderRep(object, { mode: undefined }).text()).toBe(out);
    expect(renderRep(object, { mode: MODE.TINY }).text()).toBe(
      'Promise { "fulfilled" }'
    );
    expect(renderRep(object, { mode: MODE.SHORT }).text()).toBe(out);
    expect(renderRep(object, { mode: MODE.LONG }).text()).toBe(out);
  });
});

describe("Promise - fulfilled with node", () => {
  const stub = stubs.get("FulfilledWithNode");
  const grips = getSelectableInInspectorGrips(stub);

  it("has one node grip", () => {
    expect(grips).toHaveLength(1);
  });

  it("calls the expected function on mouseover", () => {
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = renderRep(stub, { onDOMNodeMouseOver });
    const node = wrapper.find(".objectBox-node");

    node.simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver).toHaveBeenCalledWith(grips[0]);
  });

  it("calls the expected function on mouseout", () => {
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = renderRep(stub, { onDOMNodeMouseOut });
    const node = wrapper.find(".objectBox-node");

    node.simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
  });

  it("no inspect icon when the node is not connected to the DOM tree", () => {
    const renderedComponentWithoutInspectIcon = renderRep(
      stubs.get("FulfilledWithDisconnectedNode")
    );
    const node = renderedComponentWithoutInspectIcon.find(".open-inspector");

    expect(node.exists()).toBe(false);
  });

  it("renders an inspect icon", () => {
    const onInspectIconClick = jest.fn();
    const renderedComponent = renderRep(stub, { onInspectIconClick });
    const icon = renderedComponent.find(".open-inspector");

    icon.simulate("click");

    expect(icon.exists()).toBe(true);
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
  });
});
