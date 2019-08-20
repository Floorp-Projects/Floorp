/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */

const { shallow } = require("enzyme");
const { getRep, Rep } = require("../rep");
const Grip = require("../grip");
const { MODE } = require("../constants");
const stubs = require("../stubs/grip");
const gripArrayStubs = require("../stubs/grip-array");

const {
  expectActorAttribute,
  getSelectableInInspectorGrips,
  getGripLengthBubbleText,
} = require("./test-helpers");
const { maxLengthMap } = Grip;

function shallowRenderRep(object, props = {}) {
  return shallow(
    Grip.rep({
      object,
      ...props,
    })
  );
}

describe("Grip - empty object", () => {
  // Test object: `{}`
  const object = stubs.get("testBasic");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object {  }";

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe("{}");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });
});

describe("Grip - Boolean object", () => {
  // Test object: `new Boolean(true)`
  const object = stubs.get("testBooleanObject");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Boolean { true }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("Boolean");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Number object", () => {
  // Test object: `new Number(42)`
  const object = stubs.get("testNumberObject");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Number { 42 }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("Number");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - String object", () => {
  // Test object: `new String("foo")`
  const object = stubs.get("testStringObject");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = 'String { "foo" }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("String");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Proxy", () => {
  // Test object: `new Proxy({a:1},[1,2,3])`
  const object = stubs.get("testProxy");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const handler = object.preview.ownProperties["<handler>"].value;
    const handlerLength = getGripLengthBubbleText(handler, {
      mode: MODE.TINY,
    });
    const out = `Proxy { <target>: {…}, <handler>: ${handlerLength} […] }`;

    expect(renderRep({ mode: undefined }).text()).toBe(out);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("Proxy");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(out);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(out);
  });
});

describe("Grip - ArrayBuffer", () => {
  // Test object: `new ArrayBuffer(10)`
  const object = stubs.get("testArrayBuffer");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "ArrayBuffer { byteLength: 10 }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("ArrayBuffer");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - SharedArrayBuffer", () => {
  // Test object: `new SharedArrayBuffer(5)`
  const object = stubs.get("testSharedArrayBuffer");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "SharedArrayBuffer { byteLength: 5 }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("SharedArrayBuffer");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - ApplicationCache", () => {
  // Test object: `window.applicationCache`
  const object = stubs.get("testApplicationCache");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput =
      "OfflineResourceList { status: 0, onchecking: null, onerror: null, … }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("OfflineResourceList");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    const longOutput =
      "OfflineResourceList { status: 0, onchecking: null, " +
      "onerror: null, onnoupdate: null, ondownloading: null, " +
      "onprogress: null, onupdateready: null, oncached: null, " +
      "onobsolete: null, mozItems: DOMStringList [] }";
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("Grip - Object with max props", () => {
  // Test object: `{a: "a", b: "b", c: "c"}`
  const object = stubs.get("testMaxProps");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = 'Object { a: "a", b: "b", c: "c" }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with more than short mode max props", () => {
  // Test object: `{a: undefined, b: 1, more: 2, d: 3}`;
  const object = stubs.get("testMoreProp");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object { b: 1, more: 2, d: 3, … }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    const longOutput = "Object { a: undefined, b: 1, more: 2, d: 3 }";
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("Grip - Object with more than long mode max props", () => {
  // Test object = `{p0: "0", p1: "1", p2: "2", …, p100: "100"}`
  const object = stubs.get("testMoreThanMaxProps");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = 'Object { p0: "0", p1: "1", p2: "2", … }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);

    const props = Array.from({ length: maxLengthMap.get(MODE.LONG) }).map(
      (item, i) => `p${i}: "${i}"`
    );
    const longOutput = `Object { ${props.join(", ")}, … }`;
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(longOutput);
  });
});

describe("Grip - Object with non-enumerable properties", () => {
  // Test object: `Object.defineProperty({}, "foo", {enumerable : false});`
  const object = stubs.get("testNonEnumerableProps");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object { … }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with nested object", () => {
  // Test object: `{objProp: {id: 1}, strProp: "test string"}`
  const object = stubs.get("testNestedObject");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = 'Object { objProp: {…}, strProp: "test string" }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);

    // Check the custom title with nested objects to make sure nested objects
    // are not displayed with their parent's title.
    expect(
      renderRep({
        mode: MODE.LONG,
        title: "CustomTitle",
      }).text()
    ).toBe('CustomTitle { objProp: {…}, strProp: "test string" }');
  });
});

describe("Grip - Object with nested array", () => {
  // Test object: `{arrProp: ["foo", "bar", "baz"]}`
  const object = stubs.get("testNestedArray");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const propLength = getGripLengthBubbleText(
      object.preview.ownProperties.arrProp.value,
      { mode: MODE.TINY }
    );
    const defaultOutput = `Object { arrProp: ${propLength} […] }`;

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with connected nodes", () => {
  const object = stubs.get("testObjectWithNodes");
  const grips = getSelectableInInspectorGrips(object);
  const renderRep = props => shallowRenderRep(object, props);

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("has the expected number of node grip", () => {
    expect(grips).toHaveLength(2);
  });

  it("renders as expected", () => {
    const defaultOutput =
      "Object { foo: button#btn-1.btn.btn-log, bar: button#btn-2.btn.btn-err }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("calls the expected function on mouseover", () => {
    const onDOMNodeMouseOver = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOver });
    const node = wrapper.find(".objectBox-node");

    node.at(0).simulate("mouseover");
    node.at(1).simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(2);
    expect(onDOMNodeMouseOver.mock.calls[0][0]).toBe(grips[0]);
    expect(onDOMNodeMouseOver.mock.calls[1][0]).toBe(grips[1]);
  });

  it("calls the expected function on mouseout", () => {
    const onDOMNodeMouseOut = jest.fn();
    const wrapper = renderRep({ onDOMNodeMouseOut });
    const node = wrapper.find(".objectBox-node");

    node.at(0).simulate("mouseout");
    node.at(1).simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(2);
    expect(onDOMNodeMouseOut.mock.calls[0][0]).toBe(grips[0]);
    expect(onDOMNodeMouseOut.mock.calls[1][0]).toBe(grips[1]);
  });

  it("calls the expected function on click", () => {
    const onInspectIconClick = jest.fn();
    const wrapper = renderRep({ onInspectIconClick });
    const node = wrapper.find(".open-inspector");

    node.at(0).simulate("click");
    node.at(1).simulate("click");

    expect(onInspectIconClick.mock.calls).toHaveLength(2);
    expect(onInspectIconClick.mock.calls[0][0]).toBe(grips[0]);
    expect(onInspectIconClick.mock.calls[1][0]).toBe(grips[1]);
  });
});

describe("Grip - Object with disconnected nodes", () => {
  const object = stubs.get("testObjectWithDisconnectedNodes");
  const renderRep = props => shallowRenderRep(object, props);
  const grips = getSelectableInInspectorGrips(object);

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("has the expected number of grips", () => {
    expect(grips).toHaveLength(2);
  });

  it("no inspect icon when nodes are not connected to the DOM tree", () => {
    const onInspectIconClick = jest.fn();
    const wrapper = renderRep({ onInspectIconClick });

    const node = wrapper.find(".open-inspector");
    expect(node.exists()).toBe(false);
  });
});

describe("Grip - Object with getter", () => {
  const object = stubs.get("TestObjectWithGetter");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object { x: Getter }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with setter", () => {
  const object = stubs.get("TestObjectWithSetter");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object { x: Setter }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with getter and setter", () => {
  const object = stubs.get("TestObjectWithGetterAndSetter");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object { x: Getter & Setter }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - Object with symbol properties", () => {
  const object = stubs.get("TestObjectWithSymbolProperties");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput =
      'Object { x: 10, Symbol(): "first unnamed symbol", ' +
      'Symbol(): "second unnamed symbol", … }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      'Object { x: 10, Symbol(): "first unnamed symbol", ' +
        'Symbol(): "second unnamed symbol", Symbol(named): "named symbol", ' +
        "Symbol(Symbol.iterator): () }"
    );
  });
});

describe("Grip - Object with more than max symbol properties", () => {
  const object = stubs.get("TestObjectWithMoreThanMaxSymbolProperties");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput =
      'Object { Symbol(i-0): "value-0", Symbol(i-1): "value-1", ' +
      'Symbol(i-2): "value-2", … }';

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      'Object { Symbol(i-0): "value-0", Symbol(i-1): "value-1", ' +
        'Symbol(i-2): "value-2", Symbol(i-3): "value-3", ' +
        'Symbol(i-4): "value-4", Symbol(i-5): "value-5", ' +
        'Symbol(i-6): "value-6", Symbol(i-7): "value-7", ' +
        'Symbol(i-8): "value-8", Symbol(i-9): "value-9", … }'
    );
  });
});

describe("Grip - Without preview", () => {
  // Test object: `[1, "foo", {}]`
  const object = gripArrayStubs.get("testMaxProps").preview.items[2];

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Object {  }";

    let component = renderRep({ mode: undefined });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.TINY });
    expect(component.text()).toBe("{}");
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.SHORT });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);

    component = renderRep({ mode: MODE.LONG });
    expect(component.text()).toBe(defaultOutput);
    expectActorAttribute(component, object.actor);
  });
});

describe("Grip - Generator object", () => {
  // Test object:
  // function* genFunc() {
  //   var a = 5; while (a < 10) { yield a++; }
  // };
  // genFunc();
  const object = stubs.get("Generator");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "Generator {  }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("Generator");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

describe("Grip - DeadObject object", () => {
  // Test object (executed in a privileged content, like about:preferences):
  // `var s = Cu.Sandbox(null);Cu.nukeSandbox(s);s;`

  const object = stubs.get("DeadObject");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallowRenderRep(object, props);
    const defaultOutput = "DeadObject {  }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("DeadObject");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

// TODO: Re-enable and fix this test.
describe.skip("Grip - Object with __proto__ property", () => {
  const object = stubs.get("ObjectWith__proto__Property");

  it("correctly selects Grip Rep", () => {
    expect(getRep(object)).toBe(Grip.rep);
  });

  it("renders as expected", () => {
    const renderRep = props => shallow(Rep({ object, ...props }));
    const defaultOutput = "Object { __proto__: [] }";

    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("{…}");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});

// Test that object that might look like raw objects or arrays are rendered
// as grips when the `noGrip` parameter is not passed.
describe("Object - noGrip prop", () => {
  it("empty object", () => {
    expect(getRep({})).toBe(Grip.rep);
  });

  it("object with custom property", () => {
    expect(getRep({ foo: 123 })).toBe(Grip.rep);
  });

  it("empty array", () => {
    expect(getRep([])).toBe(Grip.rep);
  });

  it("array with some item", () => {
    expect(getRep([123])).toBe(Grip.rep);
  });
});
