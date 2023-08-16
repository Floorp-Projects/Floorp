/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/* global jest */
const { shallow } = require("enzyme");
const {
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");
const GripArray = require("resource://devtools/client/shared/components/reps/reps/grip-array.js");
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip-array.js");
const {
  expectActorAttribute,
  getSelectableInInspectorGrips,
  getGripLengthBubbleText,
} = require("resource://devtools/client/shared/components/test/node/components/reps/test-helpers.js");
const { maxLengthMap } = GripArray;

function shallowRenderRep(object, props = {}) {
  return shallow(
    GripArray.rep({
      object,
      ...props,
    })
  );
}

describe("GripArray - basic", () => {
  const object = stubs.get("testBasic");

  it("correctly selects GripArray Rep", () => {
    expect(getRep(object)).toBe(GripArray.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);
    const defaultOutput = `Array${length} []`;

    let component = renderRep({ mode: undefined, shouldRenderTooltip: true });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe("Array");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY, shouldRenderTooltip: true });
    expect(component.text()).toBe("[]");
    expect(component.prop("title")).toBe("Array");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER, shouldRenderTooltip: true });
    expect(component.text()).toBe("Array");
    expect(component.prop("title")).toBe("Array");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT, shouldRenderTooltip: true });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe("Array");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG, shouldRenderTooltip: true });
    expect(component.text()).toBe(defaultOutput);
    expect(component.prop("title")).toBe("Array");
    expectActorAttribute(component, object.actor);
  });
});

describe("GripArray - max props", () => {
  const object = stubs.get("testMaxProps");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);

    let length = getGripLengthBubbleText(object);
    const defaultOutput = `Array${length} [ 1, "foo", {} ]`;
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    // Check the custom title with nested objects to make sure nested objects
    // are not displayed with their parent's title.
    expect(
      renderRep({
        mode: MODE.LONG,
        title: "CustomTitle",
      }).text()
    ).toBe(`CustomTitle${length} [ 1, "foo", {} ]`);
  });
});

describe("GripArray - more than short mode max props", () => {
  const object = stubs.get("testMoreThanShortMaxProps");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const shortLength = maxLengthMap.get(MODE.SHORT);
    const shortContent = Array(shortLength).fill('"test string"').join(", ");
    const longContent = Array(shortLength + 1)
      .fill('"test string"')
      .join(", ");
    const defaultOutput = `Array${length} [ ${shortContent}, … ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      `Array${length} [ ${longContent} ]`
    );
  });
});

describe("GripArray - more than long mode max props", () => {
  const object = stubs.get("testMoreThanLongMaxProps");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const shortLength = maxLengthMap.get(MODE.SHORT);
    const longLength = maxLengthMap.get(MODE.LONG);
    const shortContent = Array(shortLength).fill('"test string"').join(", ");
    const defaultOutput = `Array${length} [ ${shortContent}, … ]`;
    const longContent = Array(longLength).fill('"test string"').join(", ");

    expect(
      renderRep({ mode: undefined, shouldRenderTooltip: true }).text()
    ).toBe(defaultOutput);
    expect(
      renderRep({ mode: undefined, shouldRenderTooltip: true }).prop("title")
    ).toBe("Array");
    expect(
      renderRep({ mode: MODE.TINY, shouldRenderTooltip: true }).text()
    ).toBe(`${length} […]`);
    expect(
      renderRep({ mode: MODE.TINY, shouldRenderTooltip: true }).prop("title")
    ).toBe("Array");
    expect(
      renderRep({ mode: MODE.HEADER, shouldRenderTooltip: true }).text()
    ).toBe(`Array${length}`);
    expect(
      renderRep({ mode: MODE.HEADER, shouldRenderTooltip: true }).prop("title")
    ).toBe("Array");
    expect(
      renderRep({ mode: MODE.SHORT, shouldRenderTooltip: true }).text()
    ).toBe(defaultOutput);
    expect(
      renderRep({ mode: MODE.SHORT, shouldRenderTooltip: true }).prop("title")
    ).toBe("Array");
    expect(
      renderRep({ mode: MODE.LONG, shouldRenderTooltip: true }).text()
    ).toBe(`Array${length} [ ${longContent}, … ]`);
    expect(
      renderRep({ mode: MODE.LONG, shouldRenderTooltip: true }).prop("title")
    ).toBe("Array");
  });
});

describe("GripArray - recursive array", () => {
  const object = stubs.get("testRecursiveArray");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const childArrayLength = getGripLengthBubbleText(object.preview.items[0], {
      mode: MODE.TINY,
    });

    const defaultOutput = `Array${length} [ ${childArrayLength} […] ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("GripArray - preview limit", () => {
  const object = stubs.get("testPreviewLimit");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const shortOutput = `Array${length} [ 0, 1, 2, … ]`;
    const longOutput = `Array${length} [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, … ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(shortOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(shortOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("GripArray - empty slots", () => {
  it("renders an array with empty slots only as expected", () => {
    const object = stubs.get("Array(5)");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const defaultOutput = `Array${length} [ <5 empty slots> ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput = `Array${length} [ <5 empty slots> ]`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("one empty slot at the beginning as expected", () => {
    const object = stubs.get("[,1,2,3]");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ <1 empty slot>, 1, 2, … ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput = `Array${length} [ <1 empty slot>, 1, 2, 3 ]`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("multiple consecutive empty slots at the beginning as expected", () => {
    const object = stubs.get("[,,,3,4,5]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ <3 empty slots>, 3, 4, … ]`;
    const longOutput = `Array${length} [ <3 empty slots>, 3, 4, 5 ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("one empty slot in the middle as expected", () => {
    const object = stubs.get("[0,1,,3,4,5]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, 1, <1 empty slot>, … ]`;
    const longOutput = `Array${length} [ 0, 1, <1 empty slot>, 3, 4, 5 ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("successive empty slots in the middle as expected", () => {
    const object = stubs.get("[0,1,,,,5]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, 1, <3 empty slots>, … ]`;
    const longOutput = `Array${length} [ 0, 1, <3 empty slots>, 5 ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("non successive single empty slots as expected", () => {
    const object = stubs.get("[0,,2,,4,5]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, <1 empty slot>, 2, … ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      `Array${length} [ 0, <1 empty slot>, 2, <1 empty slot>, 4, 5 ]`
    );
  });

  it("multiple multi-slot holes as expected", () => {
    const object = stubs.get("[0,,,3,,,,7,8]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, <2 empty slots>, 3, … ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      `Array${length} [ 0, <2 empty slots>, 3, <3 empty slots>, 7, 8 ]`
    );
  });

  it("a single slot hole at the end as expected", () => {
    const object = stubs.get("[0,1,2,3,4,,]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, 1, 2, … ]`;
    const longOutput = `Array${length} [ 0, 1, 2, 3, 4, <1 empty slot> ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("multiple consecutive empty slots at the end as expected", () => {
    const object = stubs.get("[0,1,2,,,,]");
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ 0, 1, 2, … ]`;
    const longOutput = `Array${length} [ 0, 1, 2, <3 empty slots> ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("GripArray - NamedNodeMap", () => {
  const object = stubs.get("testNamedNodeMap");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const defaultOutput =
      `NamedNodeMap${length} ` +
      '[ class="myclass", cellpadding="7", border="3" ]';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`NamedNodeMap${length}`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(
      `NamedNodeMap${length}`
    );

    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("GripArray - NodeList", () => {
  const object = stubs.get("testNodeList");
  const grips = getSelectableInInspectorGrips(object);
  const renderRep = props => shallowRenderRep(object, props);
  let length = getGripLengthBubbleText(object);

  it("renders as expected", () => {
    const defaultOutput =
      `NodeList${length} [ button#btn-1.btn.btn-log, ` +
      "button#btn-2.btn.btn-err, button#btn-3.btn.btn-count ]";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`NodeList${length}`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`NodeList${length}`);

    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("has 3 node grip", () => {
    expect(grips).toHaveLength(3);
  });

  it("calls the expected function on mouseover", () => {
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOver });
    const node = wrapper.find(".objectBox-node");

    node.at(0).simulate("mouseover");
    node.at(1).simulate("mouseover");
    node.at(2).simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(3);
    expect(onDOMNodeMouseOver.mock.calls[0][0]).toBe(grips[0]);
    expect(onDOMNodeMouseOver.mock.calls[1][0]).toBe(grips[1]);
    expect(onDOMNodeMouseOver.mock.calls[2][0]).toBe(grips[2]);
  });

  it("calls the expected function on mouseout", () => {
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOut });
    const node = wrapper.find(".objectBox-node");

    node.at(0).simulate("mouseout");
    node.at(1).simulate("mouseout");
    node.at(2).simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(3);
    expect(onDOMNodeMouseOut.mock.calls[0][0]).toBe(grips[0]);
    expect(onDOMNodeMouseOut.mock.calls[1][0]).toBe(grips[1]);
    expect(onDOMNodeMouseOut.mock.calls[2][0]).toBe(grips[2]);
  });

  it("calls the expected function on click", () => {
    const onInspectIconClick = jest.fn();
    const wrapper = renderRep({ onInspectIconClick });
    const node = wrapper.find(".open-inspector");

    node.at(0).simulate("click");
    node.at(1).simulate("click");
    node.at(2).simulate("click");

    expect(onInspectIconClick.mock.calls).toHaveLength(3);
    expect(onInspectIconClick.mock.calls[0][0]).toBe(grips[0]);
    expect(onInspectIconClick.mock.calls[1][0]).toBe(grips[1]);
    expect(onInspectIconClick.mock.calls[2][0]).toBe(grips[2]);
  });

  it("no inspect icon when nodes are not connected to the DOM tree", () => {
    const renderedComponentWithoutInspectIcon = shallowRenderRep(
      stubs.get("testDisconnectedNodeList")
    );
    const node = renderedComponentWithoutInspectIcon.find(".open-inspector");
    expect(node.exists()).toBe(false);
  });
});

describe("GripArray - DocumentFragment", () => {
  const object = stubs.get("testDocumentFragment");

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);

    let length = getGripLengthBubbleText(object);
    const defaultOutput =
      `DocumentFragment${length} [ li#li-0.list-element, ` +
      "li#li-1.list-element, li#li-2.list-element, … ]";
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(
      `DocumentFragment${length}`
    );
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(
      `DocumentFragment${length}`
    );

    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput =
      `DocumentFragment${length} [ ` +
      "li#li-0.list-element, li#li-1.list-element, li#li-2.list-element, " +
      "li#li-3.list-element, li#li-4.list-element ]";
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("GripArray - Items not in preview", () => {
  const object = stubs.get("testItemsNotInPreview");

  it("correctly selects GripArray Rep", () => {
    expect(getRep(object)).toBe(GripArray.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const defaultOutput = `Array${length} [ … ]`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(`${length} […]`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`Array${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });
});

describe("GripArray - Set", () => {
  it("correctly selects GripArray Rep", () => {
    const object = stubs.get("new Set([1,2,3,4])");
    expect(getRep(object)).toBe(GripArray.rep);
  });

  it("renders short sets as expected", () => {
    const object = stubs.get("new Set([1,2,3,4])");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const defaultOutput = `Set${length} [ 1, 2, 3, … ]`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(`Set${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`Set${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(`Set${length} [ 1, 2, 3, 4 ]`);
    expectActorAttribute(component, object.actor);
  });

  it("renders larger sets as expected", () => {
    const object = stubs.get("new Set([0,1,2,…,19])");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const defaultOutput = `Set${length} [ 0, 1, 2, … ]`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(`Set${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`Set${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(
      `Set${length} [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, … ]`
    );
    expectActorAttribute(component, object.actor);
  });
});

describe("GripArray - WeakSet", () => {
  it("correctly selects GripArray Rep", () => {
    const object = stubs.get(
      "new WeakSet(document.querySelectorAll('button:nth-child(3n)'))"
    );
    expect(getRep(object)).toBe(GripArray.rep);
  });

  it("renders short WeakSets as expected", () => {
    const object = stubs.get(
      "new WeakSet(document.querySelectorAll('button:nth-child(3n)'))"
    );
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);
    const defaultOutput = `WeakSet${length} [ button, button, button, … ]`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(`WeakSet${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`WeakSet${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(
      `WeakSet${length} [ button, button, button, button ]`
    );
    expectActorAttribute(component, object.actor);
  });

  it("renders larger WeakSets as expected", () => {
    const object = stubs.get(
      "new WeakSet(document.querySelectorAll('div, button'))"
    );
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);
    const defaultOutput = `WeakSet${length} [ button, button, button, … ]`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(`WeakSet${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`WeakSet${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(`WeakSet(12) [ ${"button, ".repeat(10)}… ]`);
    expectActorAttribute(component, object.actor);
  });
});

describe("GripArray - DOMTokenList", () => {
  const object = stubs.get("DOMTokenList");

  it("correctly selects GripArray Rep", () => {
    expect(getRep(object)).toBe(GripArray.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const length = getGripLengthBubbleText(object);
    const defaultOutput = `DOMTokenList${length} []`;

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.HEADER });
    expect(component.text()).toBe(`DOMTokenList${length}`);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });
});

describe("GripArray - accessor", () => {
  it("renders an array with getter as expected", () => {
    const object = stubs.get("TestArrayWithGetter");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ Getter ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput = `Array${length} [ Getter ]`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("renders an array with setter as expected", () => {
    const object = stubs.get("TestArrayWithSetter");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ Setter ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput = `Array${length} [ Setter ]`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });

  it("renders an array with getter and setter as expected", () => {
    const object = stubs.get("TestArrayWithGetterAndSetter");
    const renderRep = props => shallowRenderRep(object, props);
    let length = getGripLengthBubbleText(object);

    const defaultOutput = `Array${length} [ Getter & Setter ]`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(`${length} […]`);
    expect(renderRep({ mode: MODE.HEADER }).text()).toBe(`Array${length}`);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    length = getGripLengthBubbleText(object, { mode: MODE.LONG });
    const longOutput = `Array${length} [ Getter & Setter ]`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});
