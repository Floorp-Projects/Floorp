/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");
const { MODE } = require("../constants");
const { TextNode } = REPS;

const stubs = require("../stubs/text-node");
const { expectActorAttribute } = require("./test-helpers");

describe("TextNode", () => {
  it("selects TextNode Rep as expected", () => {
    expect(getRep(stubs.get("testRendering"))).toBe(TextNode.rep);
  });

  it("renders as expected", () => {
    const object = stubs.get("testRendering");
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));

    const defaultOutput = '#text "hello world"';

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe("#text");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });

  it("renders as expected with EOL", () => {
    const object = stubs.get("testRenderingWithEOL");
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));

    const defaultOutput = '#text "hello\nworld"';
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("#text");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("calls the expected function on mouseover", () => {
    const object = stubs.get("testRendering");
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onDOMNodeMouseOver }));

    wrapper.simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver).toHaveBeenCalledWith(object);
  });

  it("calls the expected function on mouseout", () => {
    const object = stubs.get("testRendering");
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onDOMNodeMouseOut }));

    wrapper.simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOut).toHaveBeenCalledWith(object);
  });

  it("displays a button when the node is connected", () => {
    const object = stubs.get("testRendering");

    const onInspectIconClick = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onInspectIconClick }));

    const inspectIconNode = wrapper.find(".open-inspector");
    expect(inspectIconNode !== null).toBe(true);

    const event = Symbol("click-event");
    inspectIconNode.simulate("click", event);

    // The function is called once
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
    const [arg1, arg2] = onInspectIconClick.mock.calls[0];
    // First argument is the grip
    expect(arg1).toBe(object);
    // Second one is the event
    expect(arg2).toBe(event);
  });

  it("does not display a button when the node is connected", () => {
    const object = stubs.get("testRenderingDisconnected");

    const onInspectIconClick = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onInspectIconClick }));
    expect(wrapper.find(".open-inspector")).toHaveLength(0);
  });
});
