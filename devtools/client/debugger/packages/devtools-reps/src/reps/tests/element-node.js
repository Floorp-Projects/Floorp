/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mount, shallow } = require("enzyme");
const { JSDOM } = require("jsdom");
const { REPS, getRep } = require("../rep");
const { MODE } = require("../constants");
const { MAX_ATTRIBUTE_LENGTH } = require("../element-node");
const { ElementNode } = REPS;
const {
  expectActorAttribute,
  getSelectableInInspectorGrips,
} = require("./test-helpers");
const { ELLIPSIS } = require("../rep-utils");
const stubs = require("../stubs/element-node");

describe("ElementNode - BodyNode", () => {
  const stub = stubs.get("BodyNode");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<body id="body-id" class="body-class">'
    );
    expectActorAttribute(renderedComponent, stub.actor);
  });

  it("renders with expected text content on tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("body#body-id.body-class");
    expectActorAttribute(renderedComponent, stub.actor);
  });
});

describe("ElementNode - DocumentElement", () => {
  const stub = stubs.get("DocumentElement");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual('<html dir="ltr" lang="en-US">');
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("html");
  });
});

describe("ElementNode - Node", () => {
  const stub = stubs.get("Node");
  const grips = getSelectableInInspectorGrips(stub);

  it("has one node grip", () => {
    expect(grips).toHaveLength(1);
  });

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<input id="newtab-customize-button" class="bar baz" dir="ltr" ' +
        'title="Customize your New Tab page" value="foo" type="button">'
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual(
      "input#newtab-customize-button.bar.baz"
    );
  });

  it("renders an inspect icon", () => {
    const onInspectIconClick = jest.fn();
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stubs.get("Node"),
        onInspectIconClick,
      })
    );

    const node = renderedComponent.find(".open-inspector");
    node.simulate("click", { type: "click" });

    expect(node.exists()).toBeTruthy();
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
    expect(onInspectIconClick.mock.calls[0][0]).toEqual(stub);
    expect(onInspectIconClick.mock.calls[0][1].type).toEqual("click");
  });

  it("calls the expected function when click is fired on Rep", () => {
    const onDOMNodeClick = jest.fn();
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        onDOMNodeClick,
      })
    );

    renderedComponent.simulate("click");

    expect(onDOMNodeClick.mock.calls).toHaveLength(1);
  });

  it("calls the expected function when mouseout is fired on Rep", () => {
    const onDOMNodeMouseOut = jest.fn();
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        onDOMNodeMouseOut,
      })
    );

    renderedComponent.simulate("mouseout");

    expect(onDOMNodeMouseOut.mock.calls).toHaveLength(1);
  });

  it("calls the expected function when mouseover is fired on Rep", () => {
    const onDOMNodeMouseOver = jest.fn();
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        onDOMNodeMouseOver,
      })
    );

    renderedComponent.simulate("mouseover");

    expect(onDOMNodeMouseOver.mock.calls).toHaveLength(1);
    expect(onDOMNodeMouseOver.mock.calls[0][0]).toEqual(stub);
  });
});

describe("ElementNode - Leading and trailing spaces class name", () => {
  const stub = stubs.get("NodeWithLeadingAndTrailingSpacesClassName");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<body id="nightly-whatsnew" class="  html-ltr    ">'
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("body#nightly-whatsnew.html-ltr");
  });
});

describe("ElementNode - Node with spaces in the class name", () => {
  const stub = stubs.get("NodeWithSpacesInClassName");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent).toMatchSnapshot();
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("body.a.b.c");
  });
});

describe("ElementNode - Node without attributes", () => {
  const stub = stubs.get("NodeWithoutAttributes");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("<p>");
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("p");
  });
});

describe("ElementNode - Node with many attributes", () => {
  const stub = stubs.get("LotsOfAttributes");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<p id="lots-of-attributes" a="" b="" c="" d="" e="" f="" g="" ' +
        'h="" i="" j="" k="" l="" m="" n="">'
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("p#lots-of-attributes");
  });
});

describe("ElementNode - SVG Node", () => {
  const stub = stubs.get("SvgNode");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<clipPath id="clip" class="svg-element">'
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("clipPath#clip.svg-element");
  });
});

describe("ElementNode - SVG Node in XHTML", () => {
  const stub = stubs.get("SvgNodeInXHTML");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<svg:circle class="svg-element" cx="0" cy="0" r="5">'
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("svg:circle.svg-element");
  });
});

describe("ElementNode - Disconnected node", () => {
  const stub = stubs.get("DisconnectedNode");

  it("renders no inspect icon when the node is not in the DOM tree", () => {
    const onInspectIconClick = jest.fn();
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        onInspectIconClick,
      })
    );

    expect(renderedComponent.find(".open-inspector").exists()).toBeFalsy();
  });
});

describe("ElementNode - Element with longString attribute", () => {
  const stub = stubs.get("NodeWithLongStringAttribute");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      `<div data-test="${"a".repeat(MAX_ATTRIBUTE_LENGTH)}${ELLIPSIS}">`
    );
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("div");
  });
});

describe("ElementNode - Element attribute cropping", () => {
  it("renders no title attribute for short attribute", () => {
    const stub = stubs.get("NodeWithSpacesInClassName");
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );
    expect(
      renderedComponent
        .first()
        .find("span.attrValue")
        .prop("title")
    ).toBe(undefined);
  });

  it("renders partial value for long attribute", () => {
    const stub = stubs.get("NodeWithLongAttribute");

    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<p data-test="aaaaaaaaaaaaaaaaaaaaaaaa…aaaaaaaaaaaaaaaaaaaaaaa">'
    );
    expect(
      renderedComponent
        .first()
        .find("span.attrValue")
        .prop("title")
    ).toBe("a".repeat(100));
  });

  it("renders partial attribute for LongString", () => {
    const stub = stubs.get("NodeWithLongStringAttribute");

    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '<div data-test="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa…">'
    );
    expect(
      renderedComponent
        .first()
        .find("span.attrValue")
        .prop("title")
    ).toBe("a".repeat(1000));
  });
});

describe("ElementNode - : Marker pseudo element", () => {
  const stub = stubs.get("MarkerPseudoElement");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("::marker");
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("::marker");
  });
});

describe("ElementNode - : Before pseudo element", () => {
  const stub = stubs.get("BeforePseudoElement");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("::before");
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("::before");
  });
});

describe("ElementNode - After pseudo element", () => {
  const stub = stubs.get("AfterPseudoElement");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(ElementNode.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("::after");
  });

  it("renders with expected text content in tiny mode", () => {
    const renderedComponent = shallow(
      ElementNode.rep({
        object: stub,
        mode: MODE.TINY,
      })
    );

    expect(renderedComponent.text()).toEqual("::after");
  });
});

describe("ElementNode - Inspect icon title", () => {
  const stub = stubs.get("Node");

  it("renders with expected title", () => {
    const inspectIconTitle = "inspect icon title";

    const renderedComponent = shallow(
      ElementNode.rep({
        inspectIconTitle,
        object: stub,
        onInspectIconClick: jest.fn(),
      })
    );

    const iconNode = renderedComponent.find(".open-inspector");
    expect(iconNode.prop("title")).toEqual(inspectIconTitle);
  });
});

describe("ElementNode - Cursor style", () => {
  const stub = stubs.get("Node");

  it("renders with styled cursor", async () => {
    const window = await createWindowForCursorTest();
    const attachTo = window.document.querySelector("#attach-to");
    const renderedComponent = mount(
      ElementNode.rep({
        object: stub,
        onDOMNodeClick: jest.fn(),
        onInspectIconClick: jest.fn(),
      }),
      {
        attachTo,
      }
    );

    const objectNode = renderedComponent.getDOMNode();
    const iconNode = objectNode.querySelector(".open-inspector");
    expect(renderedComponent.hasClass("clickable")).toBeTruthy();
    expect(window.getComputedStyle(objectNode).cursor).toEqual("pointer");
    expect(window.getComputedStyle(iconNode).cursor).toEqual("pointer");
  });

  it("renders with unstyled cursor", async () => {
    const window = await createWindowForCursorTest();
    const attachTo = window.document.querySelector("#attach-to");
    const renderedComponent = mount(
      ElementNode.rep({
        object: stub,
      }),
      {
        attachTo,
      }
    );

    const objectNode = renderedComponent.getDOMNode();
    expect(renderedComponent.hasClass("clickable")).toBeFalsy();
    expect(window.getComputedStyle(objectNode).cursor).toEqual("");
  });
});

async function createWindowForCursorTest() {
  const path = require("path");
  const css = await readTextFile(path.resolve(__dirname, "..", "reps.css"));
  const html = `
    <body>
      <style>${css}</style>
      <div id="attach-to"></div>
    </body>
    `;

  return new JSDOM(html).window;
}

async function readTextFile(fileName) {
  return new Promise((resolve, reject) => {
    const fs = require("fs");
    fs.readFile(fileName, "utf8", (error, text) => {
      if (error) {
        reject(error);
      } else {
        resolve(text);
      }
    });
  });
}
