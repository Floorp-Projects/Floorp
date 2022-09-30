/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");
const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");
const { Obj } = REPS;
const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");

const renderComponent = (object, props) => {
  return shallow(Obj.rep({ object, ...props }));
};

describe("Object - Basic", () => {
  const object = {};
  const defaultOutput = "Object {  }";

  it("selects the correct rep", () => {
    expect(getRep(object, undefined, true)).toBe(Obj.rep);
  });

  it("renders basic object as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: undefined }).prop("title")).toEqual(
      "Object"
    );

    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{}");
    expect(renderComponent(object, { mode: MODE.TINY }).prop("title")).toEqual(
      "Object"
    );

    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Max props", () => {
  const object = { a: "a", b: "b", c: "c" };
  const defaultOutput = 'Object { a: "a", b: "b", c: "c" }';

  it("renders object with max props as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{…}");
    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Many props", () => {
  const object = {};
  for (let i = 0; i < 100; i++) {
    object[`p${i}`] = i;
  }
  const defaultOutput = "Object { p0: 0, p1: 1, p2: 2, … }";

  it("renders object with many props as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{…}");
    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Uninteresting props", () => {
  const object = { a: undefined, b: undefined, c: "c", d: 0 };
  const defaultOutput = 'Object { c: "c", d: 0, a: undefined, … }';

  it("renders object with uninteresting props as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{…}");
    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Escaped property names", () => {
  const object = { "": 1, "quote-this": 2, noquotes: 3 };
  const defaultOutput = 'Object { "": 1, "quote-this": 2, noquotes: 3 }';

  it("renders object with escaped property names as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{…}");
    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Nested", () => {
  const object = {
    objProp: {
      id: 1,
      arr: [2],
    },
    strProp: "test string",
    arrProp: [1],
  };
  const defaultOutput =
    'Object { strProp: "test string", objProp: {…},' + " arrProp: […] }";

  it("renders nested object as expected", () => {
    expect(
      renderComponent(object, { mode: undefined, noGrip: true }).text()
    ).toEqual(defaultOutput);
    expect(
      renderComponent(object, { mode: MODE.TINY, noGrip: true }).text()
    ).toEqual("{…}");
    expect(
      renderComponent(object, { mode: MODE.SHORT, noGrip: true }).text()
    ).toEqual(defaultOutput);
    expect(
      renderComponent(object, { mode: MODE.LONG, noGrip: true }).text()
    ).toEqual(defaultOutput);
  });
});

describe("Object - More prop", () => {
  const object = {
    a: undefined,
    b: 1,
    more: 2,
    d: 3,
  };
  const defaultOutput = "Object { b: 1, more: 2, d: 3, … }";

  it("renders object with more properties as expected", () => {
    expect(renderComponent(object, { mode: undefined }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.TINY }).text()).toEqual("{…}");
    expect(renderComponent(object, { mode: MODE.SHORT }).text()).toEqual(
      defaultOutput
    );
    expect(renderComponent(object, { mode: MODE.LONG }).text()).toEqual(
      defaultOutput
    );
  });
});

describe("Object - Custom Title", () => {
  const customTitle = "MyCustomObject";
  const object = { a: "a", b: "b", c: "c" };
  const defaultOutput = `${customTitle} { a: "a", b: "b", c: "c" }`;

  it("renders object with more properties as expected", () => {
    expect(
      renderComponent(object, { mode: undefined, title: customTitle }).text()
    ).toEqual(defaultOutput);
    expect(
      renderComponent(object, { mode: undefined, title: customTitle }).prop(
        "title"
      )
    ).toEqual(customTitle);
    expect(
      renderComponent(object, { mode: MODE.TINY, title: customTitle }).text()
    ).toEqual(customTitle);
    expect(
      renderComponent(object, { mode: MODE.TINY, title: customTitle }).prop(
        "title"
      )
    ).toEqual(customTitle);
    expect(
      renderComponent(object, { mode: MODE.SHORT, title: customTitle }).text()
    ).toEqual(defaultOutput);
    expect(
      renderComponent(object, { mode: MODE.LONG, title: customTitle }).text()
    ).toEqual(defaultOutput);
  });
});

// Test that object that might look like Grips are rendered as Object when
// passed the `noGrip` property.
describe("Object - noGrip prop", () => {
  it("object with type property", () => {
    expect(getRep({ type: "string" }, undefined, true)).toBe(Obj.rep);
  });

  it("object with actor property", () => {
    expect(getRep({ actor: "fake/actorId" }, undefined, true)).toBe(Obj.rep);
  });

  it("Attribute grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/attribute.js");
    expect(getRep(stubs.get("Attribute"), undefined, true)).toBe(Obj.rep);
  });

  it("CommentNode grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/comment-node.js");
    expect(getRep(stubs.get("Comment"), undefined, true)).toBe(Obj.rep);
  });

  it("DateTime grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/date-time.js");
    expect(getRep(stubs.get("DateTime"), undefined, true)).toBe(Obj.rep);
  });

  it("Document grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/document.js");
    expect(getRep(stubs.get("Document"), undefined, true)).toBe(Obj.rep);
  });

  it("ElementNode grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/element-node.js");
    expect(getRep(stubs.get("BodyNode"), undefined, true)).toBe(Obj.rep);
  });

  it("Error grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/error.js");
    expect(getRep(stubs.get("SimpleError"), undefined, true)).toBe(Obj.rep);
  });

  it("Event grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/event.js");
    expect(getRep(stubs.get("testEvent"), undefined, true)).toBe(Obj.rep);
  });

  it("Function grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/function.js");
    expect(getRep(stubs.get("Named"), undefined, true)).toBe(Obj.rep);
  });

  it("Array grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip-array.js");
    expect(getRep(stubs.get("testMaxProps"), undefined, true)).toBe(Obj.rep);
  });

  it("Map grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip-map.js");
    expect(getRep(stubs.get("testSymbolKeyedMap"), undefined, true)).toBe(
      Obj.rep
    );
  });

  it("Object grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/grip.js");
    expect(getRep(stubs.get("testMaxProps"), undefined, true)).toBe(Obj.rep);
  });

  it("Infinity grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/infinity.js");
    expect(getRep(stubs.get("Infinity"), undefined, true)).toBe(Obj.rep);
  });

  it("LongString grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/long-string.js");
    expect(getRep(stubs.get("testMultiline"), undefined, true)).toBe(Obj.rep);
  });

  it("NaN grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/nan.js");
    expect(getRep(stubs.get("NaN"), undefined, true)).toBe(Obj.rep);
  });

  it("Null grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/null.js");
    expect(getRep(stubs.get("Null"), undefined, true)).toBe(Obj.rep);
  });

  it("Number grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/number.js");
    expect(getRep(stubs.get("NegZeroGrip"), undefined, true)).toBe(Obj.rep);
  });

  it("ObjectWithText grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/object-with-text.js");
    expect(getRep(stubs.get("ShadowRule"), undefined, true)).toBe(Obj.rep);
  });

  it("ObjectWithURL grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/object-with-url.js");
    expect(getRep(stubs.get("ObjectWithUrl"), undefined, true)).toBe(Obj.rep);
  });

  it("Promise grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/promise.js");
    expect(getRep(stubs.get("Pending"), undefined, true)).toBe(Obj.rep);
  });

  it("RegExp grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/regexp.js");
    expect(getRep(stubs.get("RegExp"), undefined, true)).toBe(Obj.rep);
  });

  it("Stylesheet grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/stylesheet.js");
    expect(getRep(stubs.get("StyleSheet"), undefined, true)).toBe(Obj.rep);
  });

  it("Symbol grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/symbol.js");
    expect(getRep(stubs.get("Symbol"), undefined, true)).toBe(Obj.rep);
  });

  it("TextNode grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/text-node.js");
    expect(getRep(stubs.get("testRendering"), undefined, true)).toBe(Obj.rep);
  });

  it("Undefined grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/undefined.js");
    expect(getRep(stubs.get("Undefined"), undefined, true)).toBe(Obj.rep);
  });

  it("Window grip", () => {
    const stubs = require("resource://devtools/client/shared/components/test/node/stubs/reps/window.js");
    expect(getRep(stubs.get("Window"), undefined, true)).toBe(Obj.rep);
  });

  it("Object with class property", () => {
    const object = {
      class: "Array",
    };
    expect(getRep(object, undefined, true)).toBe(Obj.rep);

    expect(
      renderComponent(object, { mode: MODE.SHORT, noGrip: true }).text()
    ).toEqual('Object { class: "Array" }');
  });
});
