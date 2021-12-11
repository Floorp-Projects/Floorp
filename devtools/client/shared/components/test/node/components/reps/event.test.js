/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global jest */
const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");
const { Event } = REPS;
const {
  expectActorAttribute,
  getSelectableInInspectorGrips,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

const {
  MODE,
} = require("devtools/client/shared/components/reps/reps/constants");
const stubs = require("devtools/client/shared/components/test/node/stubs/reps/event");

describe("Event - beforeprint", () => {
  const object = stubs.get("testEvent");

  it("correctly selects Event Rep", () => {
    expect(getRep(object)).toBe(Event.rep);
  });

  it("renders with expected text", () => {
    const renderedComponent = shallow(Event.rep({ object }));
    expect(renderedComponent.text()).toEqual(
      "beforeprint { target: Window, isTrusted: true, currentTarget: Window, " +
        "… }"
    );
    expectActorAttribute(renderedComponent, object.actor);
  });
});

describe("Event - keyboard event", () => {
  const object = stubs.get("testKeyboardEvent");

  it("correctly selects Event Rep", () => {
    expect(getRep(object)).toBe(Event.rep);
  });

  it("renders with expected text", () => {
    const renderRep = props => shallow(Event.rep({ object, ...props }));
    expect(renderRep().text()).toEqual(
      'keyup { target: body, key: "Control", charCode: 0, … }'
    );
    expect(renderRep({ mode: MODE.LONG }).text()).toEqual(
      'keyup { target: body, key: "Control", charCode: 0, keyCode: 17 }'
    );
  });
});

describe("Event - keyboard event with modifiers", () => {
  const object = stubs.get("testKeyboardEventWithModifiers");

  it("correctly selects Event Rep", () => {
    expect(getRep(object)).toBe(Event.rep);
  });

  it("renders with expected text", () => {
    const renderRep = props => shallow(Event.rep({ object, ...props }));
    expect(renderRep({ mode: MODE.LONG }).text()).toEqual(
      'keyup Meta-Shift { target: body, key: "M", charCode: 0, keyCode: 77 }'
    );
  });
});

describe("Event - message event", () => {
  const object = stubs.get("testMessageEvent");

  it("correctly selects Event Rep", () => {
    expect(getRep(object)).toBe(Event.rep);
  });

  it("renders with expected text", () => {
    const renderRep = props => shallow(Event.rep({ object, ...props }));
    expect(renderRep().text()).toEqual(
      'message { target: Window, isTrusted: false, data: "test data", … }'
    );
    expect(renderRep({ mode: MODE.LONG }).text()).toEqual(
      'message { target: Window, isTrusted: false, data: "test data", ' +
        'origin: "null", lastEventId: "", source: Window, ports: Array, ' +
        "currentTarget: Window, eventPhase: 2, bubbles: false, … }"
    );
  });
});

describe("Event - mouse event", () => {
  const object = stubs.get("testMouseEvent");
  const renderRep = props => shallow(Event.rep({ object, ...props }));

  const grips = getSelectableInInspectorGrips(object);

  it("has stub with one node grip", () => {
    expect(grips).toHaveLength(1);
  });

  it("correctly selects Event Rep", () => {
    expect(getRep(object)).toBe(Event.rep);
  });

  it("renders with expected text", () => {
    expect(renderRep({ shouldRenderTooltip: true }).text()).toEqual(
      "click { target: div#test, clientX: 62, clientY: 18, … }"
    );
    expect(renderRep({ shouldRenderTooltip: true }).prop("title")).toEqual(
      "click"
    );
    expect(
      renderRep({ mode: MODE.LONG, shouldRenderTooltip: true }).text()
    ).toEqual(
      "click { target: div#test, buttons: 0, clientX: 62, clientY: 18, " +
        "layerX: 0, layerY: 0 }"
    );
    expect(
      renderRep({ mode: MODE.LONG, shouldRenderTooltip: true }).prop("title")
    ).toEqual("click");
  });

  it("renders an inspect icon", () => {
    const onInspectIconClick = jest.fn();
    const renderedComponent = renderRep({ onInspectIconClick });

    const node = renderedComponent.find(".open-inspector");
    node.simulate("click", { type: "click" });

    expect(node.exists()).toBeTruthy();
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
    expect(onInspectIconClick.mock.calls[0][0]).toEqual(grips[0]);
    expect(onInspectIconClick.mock.calls[0][1].type).toEqual("click");
  });

  it("calls the expected function when mouseout is fired on Rep", () => {
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOut });

    const node = wrapper.find(".objectBox-node");
    node.simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOut.mock.calls[0][0]).toEqual(grips[0]);
  });

  it("calls the expected function when mouseover is fired on Rep", () => {
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOver });

    const node = wrapper.find(".objectBox-node");
    node.simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver.mock.calls[0][0]).toEqual(grips[0]);
  });
});
