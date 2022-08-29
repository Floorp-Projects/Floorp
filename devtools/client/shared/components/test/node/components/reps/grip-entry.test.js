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

const { GripEntry } = REPS;
const {
  MODE,
} = require("devtools/client/shared/components/reps/reps/constants");
const {
  createGripMapEntry,
  getGripLengthBubbleText,
} = require("devtools/client/shared/components/test/node/components/reps/test-helpers");

const stubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-entry");
const nodeStubs = require("devtools/client/shared/components/test/node/stubs/reps/element-node");
const gripArrayStubs = require("devtools/client/shared/components/test/node/stubs/reps/grip-array");

const renderRep = (object, mode, props) => {
  return shallow(
    GripEntry.rep({
      object,
      mode,
      ...props,
    })
  );
};

describe("GripEntry - simple", () => {
  const stub = stubs.get("A → 0");

  it("Rep correctly selects GripEntry Rep", () => {
    expect(getRep(stub)).toBe(GripEntry.rep);
  });

  it("GripEntry rep has expected text content", () => {
    const renderedComponent = renderRep(stub);
    expect(renderedComponent.text()).toEqual("A → 0");
  });
});

describe("GripEntry - createGripMapEntry", () => {
  it("return the expected object", () => {
    const entry = createGripMapEntry("A", 0);
    expect(entry).toEqual(stubs.get("A → 0"));
  });
});

describe("GripEntry - complex", () => {
  it("Handles complex objects as key and value", () => {
    let stub = gripArrayStubs.get("testBasic");
    let length = getGripLengthBubbleText(stub);
    let entry = createGripMapEntry("A", stub);
    expect(renderRep(entry, MODE.TINY).text()).toEqual("A → []");
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `A → Array${length} []`
    );
    expect(renderRep(entry, MODE.LONG).text()).toEqual(`A → Array${length} []`);

    entry = createGripMapEntry(stub, "A");
    expect(renderRep(entry, MODE.TINY).text()).toEqual('[] → "A"');
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `Array${length} [] → "A"`
    );
    expect(renderRep(entry, MODE.LONG).text()).toEqual(
      `Array${length} [] → "A"`
    );

    stub = gripArrayStubs.get("testMaxProps");
    length = getGripLengthBubbleText(stub, { mode: MODE.TINY });
    entry = createGripMapEntry("A", stub);
    expect(renderRep(entry, MODE.TINY).text()).toEqual(`A → ${length} […]`);
    length = getGripLengthBubbleText(stub);
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `A → Array${length} [ 1, "foo", {} ]`
    );
    length = getGripLengthBubbleText(stub, { mode: MODE.LONG });
    expect(renderRep(entry, MODE.LONG).text()).toEqual(
      `A → Array${length} [ 1, "foo", {} ]`
    );

    entry = createGripMapEntry(stub, "A");
    length = getGripLengthBubbleText(stub, { mode: MODE.TINY });
    expect(renderRep(entry, MODE.TINY).text()).toEqual(`${length} […] → "A"`);
    length = getGripLengthBubbleText(stub, { mode: MODE.SHORT });
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `Array${length} [ 1, "foo", {} ] → "A"`
    );
    length = getGripLengthBubbleText(stub, { mode: MODE.LONG });
    expect(renderRep(entry, MODE.LONG).text()).toEqual(
      `Array${length} [ 1, "foo", {} ] → "A"`
    );

    stub = gripArrayStubs.get("testMoreThanShortMaxProps");
    length = getGripLengthBubbleText(stub);
    entry = createGripMapEntry("A", stub);
    length = getGripLengthBubbleText(stub, { mode: MODE.TINY });
    expect(renderRep(entry, MODE.TINY).text()).toEqual(`A → ${length} […]`);
    length = getGripLengthBubbleText(stub, { mode: MODE.SHORT });
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `A → Array${length} [ "test string", "test string", "test string", … ]`
    );
    length = getGripLengthBubbleText(stub, { mode: MODE.LONG });
    expect(renderRep(entry, MODE.LONG).text()).toEqual(
      `A → Array${length} [ "test string", "test string", "test string",\
 "test string" ]`
    );

    entry = createGripMapEntry(stub, "A");
    length = getGripLengthBubbleText(stub, { mode: MODE.TINY });
    expect(renderRep(entry, MODE.TINY).text()).toEqual(`${length} […] → "A"`);
    length = getGripLengthBubbleText(stub, { mode: MODE.SHORT });
    expect(renderRep(entry, MODE.SHORT).text()).toEqual(
      `Array${length} [ "test string", "test string", "test string", … ] → "A"`
    );
    length = getGripLengthBubbleText(stub, { mode: MODE.LONG });
    expect(renderRep(entry, MODE.LONG).text()).toEqual(
      `Array${length} [ "test string", "test string", "test string", ` +
        '"test string" ] → "A"'
    );
  });

  it("Handles Element Nodes as key and value", () => {
    const stub = nodeStubs.get("Node");

    const onInspectIconClick = jest.fn();
    const onDOMNodeMouseOut = jest.fn();
    const onDOMNodeMouseOver = jest.fn();

    let entry = createGripMapEntry("A", stub);
    let renderedComponent = renderRep(entry, MODE.TINY, {
      onInspectIconClick,
      onDOMNodeMouseOut,
      onDOMNodeMouseOver,
    });
    expect(renderRep(entry, MODE.TINY).text()).toEqual(
      "A → input#newtab-customize-button.bar.baz"
    );

    let node = renderedComponent.find(".objectBox-node");
    let icon = node.find(".open-inspector");
    icon.simulate("click", { type: "click" });
    expect(icon.exists()).toBeTruthy();
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
    expect(onInspectIconClick.mock.calls[0][0]).toEqual(stub);
    expect(onInspectIconClick.mock.calls[0][1].type).toEqual("click");

    node.simulate("mouseout");
    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOut.mock.calls[0][0]).toEqual(stub);

    node.simulate("mouseover");
    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver.mock.calls[0][0]).toEqual(stub);

    entry = createGripMapEntry(stub, "A");
    renderedComponent = renderRep(entry, MODE.TINY, {
      onInspectIconClick,
      onDOMNodeMouseOut,
      onDOMNodeMouseOver,
    });
    expect(renderRep(entry, MODE.TINY).text()).toEqual(
      'input#newtab-customize-button.bar.baz → "A"'
    );

    node = renderedComponent.find(".objectBox-node");
    icon = node.find(".open-inspector");
    icon.simulate("click", { type: "click" });
    expect(node.exists()).toBeTruthy();
    expect(onInspectIconClick.mock.calls).toHaveLength(2);
    expect(onInspectIconClick.mock.calls[1][0]).toEqual(stub);
    expect(onInspectIconClick.mock.calls[1][1].type).toEqual("click");

    node.simulate("mouseout");
    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(2);
    expect(onDOMNodeMouseOut.mock.calls[1][0]).toEqual(stub);

    node.simulate("mouseover");
    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(2);
    expect(onDOMNodeMouseOver.mock.calls[1][0]).toEqual(stub);
  });
});
