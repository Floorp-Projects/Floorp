/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global jest */
const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const { TextNode } = REPS;

const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/text-node.js");
const {
  expectActorAttribute,
} = require("resource://devtools/client/shared/components/test/node/components/reps/test-helpers.js");
const {
  ELLIPSIS,
} = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");

function quoteNewlines(text) {
  return text.split("\n").join("\\n");
}

describe("TextNode", () => {
  it("selects TextNode Rep as expected", () => {
    expect(getRep(stubs.get("testRendering")._grip)).toBe(TextNode.rep);
  });

  it("renders as expected", () => {
    const object = stubs.get("testRendering")._grip;
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));

    const defaultOutput = '#text "hello world"';

    let component = renderRep({ shouldRenderTooltip: true, mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.TINY });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.HEADER });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });

  it("renders as expected with EOL", () => {
    const object = stubs.get("testRenderingWithEOL")._grip;
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));

    const defaultOutput = quoteNewlines('#text "hello\nworld"');
    const defaultTooltip = '#text "hello\nworld"';

    let component = renderRep({ shouldRenderTooltip: true, mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.TINY });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.HEADER });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);
  });

  it("renders as expected with double quote", () => {
    const object = stubs.get("testRenderingWithDoubleQuote")._grip;
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));

    const defaultOutput = "#text 'hello\"world'";
    const defaultTooltip = '#text "hello"world"';

    let component = renderRep({ shouldRenderTooltip: true, mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.TINY });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.HEADER });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);
  });

  it("renders as expected with long string", () => {
    const object = stubs.get("testRenderingWithLongString")._grip;
    const renderRep = props => shallow(TextNode.rep({ object, ...props }));
    const initialString = object.preview.textContent.initial;

    const defaultOutput = `#text "${quoteNewlines(initialString)}${ELLIPSIS}"`;
    const defaultTooltip = `#text "${initialString}"`;

    let component = renderRep({ shouldRenderTooltip: true, mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.TINY });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.HEADER });
    expect(component.text()).toBe("#text");
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);

    component = renderRep({ shouldRenderTooltip: true, mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe(defaultTooltip);
  });

  it("calls the expected function on mouseover", () => {
    const object = stubs.get("testRendering")._grip;
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onDOMNodeMouseOver }));

    wrapper.simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver).toHaveBeenCalledWith(object);
  });

  it("calls the expected function on mouseout", () => {
    const object = stubs.get("testRendering")._grip;
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onDOMNodeMouseOut }));

    wrapper.simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOut).toHaveBeenCalledWith(object);
  });

  it("displays a button when the node is connected", () => {
    const object = stubs.get("testRendering")._grip;

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
    const object = stubs.get("testRenderingDisconnected")._grip;

    const onInspectIconClick = jest.fn();
    const wrapper = shallow(TextNode.rep({ object, onInspectIconClick }));
    expect(wrapper.find(".open-inspector")).toHaveLength(0);
  });
});
