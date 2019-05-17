/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mount, shallow } = require("enzyme");
const { JSDOM } = require("jsdom");
const { REPS, getRep } = require("../rep");
const { Accessible } = REPS;
const { ELLIPSIS } = require("../rep-utils");
const stubs = require("../stubs/accessible");

describe("Accessible - Document", () => {
  const stub = stubs.get("Document");

  it("selects Accessible Rep", () => {
    expect(getRep(stub)).toBe(Accessible.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual('"New Tab": document');
  });
});

describe("Accessible - ButtonMenu", () => {
  const stub = stubs.get("ButtonMenu");

  it("selects Accessible Rep", () => {
    expect(getRep(stub)).toBe(Accessible.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      '"New to Nightly? Let’s get started.": buttonmenu'
    );
  });

  it("renders an inspect icon", () => {
    const onInspectIconClick = jest.fn();
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
        onInspectIconClick,
      })
    );

    const node = renderedComponent.find(".open-accessibility-inspector");
    node.simulate("click", { type: "click" });

    expect(node.exists()).toBeTruthy();
    expect(onInspectIconClick.mock.calls).toHaveLength(1);
    expect(onInspectIconClick.mock.calls[0][0]).toEqual(stub);
    expect(onInspectIconClick.mock.calls[0][1].type).toEqual("click");
  });

  it("calls the expected function when click is fired on Rep", () => {
    const onAccessibleClick = jest.fn();
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
        onAccessibleClick,
      })
    );

    renderedComponent.simulate("click");

    expect(onAccessibleClick.mock.calls).toHaveLength(1);
  });

  it("calls the expected function when mouseout is fired on Rep", () => {
    const onAccessibleMouseOut = jest.fn();
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
        onAccessibleMouseOut,
      })
    );

    renderedComponent.simulate("mouseout");

    expect(onAccessibleMouseOut.mock.calls).toHaveLength(1);
  });

  it("calls the expected function when mouseover is fired on Rep", () => {
    const onAccessibleMouseOver = jest.fn();
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
        onAccessibleMouseOver,
      })
    );

    renderedComponent.simulate("mouseover");

    expect(onAccessibleMouseOver.mock.calls).toHaveLength(1);
    expect(onAccessibleMouseOver.mock.calls[0][0]).toEqual(stub);
  });
});

describe("Accessible - No Name Accessible", () => {
  const stub = stubs.get("NoName");

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual("text container");
    expect(renderedComponent.find(".separator").exists()).toBeFalsy();
    expect(renderedComponent.find(".accessible-namer").exists()).toBeFalsy();
  });
});

describe("Accessible - Disconnected accessible", () => {
  const stub = stubs.get("DisconnectedAccessible");

  it(
    "renders no inspect icon when the accessible is not in the Accessible " +
      "tree",
    () => {
      const onInspectIconClick = jest.fn();
      const renderedComponent = shallow(
        Accessible.rep({
          object: stub,
          onInspectIconClick,
        })
      );

      expect(
        renderedComponent.find(".open-accessibility-inspector").exists()
      ).toBeFalsy();
    }
  );
});

describe("Accessible - No Preview (not a valid grip)", () => {
  const stub = stubs.get("NoPreview");

  it("does not select Accessible Rep", () => {
    expect(getRep(stub)).not.toBe(Accessible.rep);
  });
});

describe("Accessible - Accessible with long name", () => {
  const stub = stubs.get("AccessibleWithLongName");

  it("selects ElementNode Rep", () => {
    expect(getRep(stub)).toBe(Accessible.rep);
  });

  it("renders with expected text content", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual(
      `"${"a".repeat(1000)}": text leaf`
    );
  });

  it("renders with expected text content with name max length", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        object: stub,
        nameMaxLength: 20,
      })
    );

    expect(renderedComponent.text()).toEqual(
      `"${"a".repeat(9)}${ELLIPSIS}${"a".repeat(8)}": text leaf`
    );
  });
});

describe("Accessible - Inspect icon title", () => {
  const stub = stubs.get("PushButton");

  it("renders with expected title", () => {
    const inspectIconTitle = "inspect icon title";

    const renderedComponent = shallow(
      Accessible.rep({
        inspectIconTitle,
        object: stub,
        onInspectIconClick: jest.fn(),
      })
    );

    const iconNode = renderedComponent.find(".open-accessibility-inspector");
    expect(iconNode.prop("title")).toEqual(inspectIconTitle);
  });
});

describe("Accessible - Separator text", () => {
  const stub = stubs.get("PushButton");

  it("renders with expected title", () => {
    const separatorText = " - ";

    const renderedComponent = shallow(
      Accessible.rep({
        separatorText,
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual('"Search" - pushbutton');
  });
});

describe("Accessible - Role first", () => {
  const stub = stubs.get("PushButton");

  it("renders with expected title", () => {
    const renderedComponent = shallow(
      Accessible.rep({
        roleFirst: true,
        object: stub,
      })
    );

    expect(renderedComponent.text()).toEqual('pushbutton: "Search"');
  });
});

describe("Accessible - Cursor style", () => {
  const stub = stubs.get("PushButton");

  it("renders with styled cursor", async () => {
    const window = await createWindowForCursorTest();
    const attachTo = window.document.querySelector("#attach-to");
    const renderedComponent = mount(
      Accessible.rep({
        object: stub,
        onAccessibleClick: jest.fn(),
        onInspectIconClick: jest.fn(),
      }),
      {
        attachTo,
      }
    );

    const objectNode = renderedComponent.getDOMNode();
    const iconNode = objectNode.querySelector(".open-accessibility-inspector");
    expect(renderedComponent.hasClass("clickable")).toBeTruthy();
    expect(window.getComputedStyle(objectNode).cursor).toEqual("pointer");
    expect(window.getComputedStyle(iconNode).cursor).toEqual("pointer");
  });

  it("renders with unstyled cursor", async () => {
    const window = await createWindowForCursorTest();
    const attachTo = window.document.querySelector("#attach-to");
    const renderedComponent = mount(
      Accessible.rep({
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
