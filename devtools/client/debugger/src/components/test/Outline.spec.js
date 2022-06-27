/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";
import Outline from "../../components/PrimaryPanes/Outline";
import { makeSymbolDeclaration } from "../../utils/test-head";
import { mockcx } from "../../utils/test-mockup";
import { showMenu } from "../../context-menu/menu";
import { copyToTheClipboard } from "../../utils/clipboard";

jest.mock("../../context-menu/menu", () => ({ showMenu: jest.fn() }));
jest.mock("../../utils/clipboard", () => ({ copyToTheClipboard: jest.fn() }));

const sourceId = "id";
const mockFunctionText = "mock function text";

function generateDefaults(overrides) {
  return {
    cx: mockcx,
    selectLocation: jest.fn(),
    selectedSource: { id: sourceId },
    getFunctionText: jest.fn().mockReturnValue(mockFunctionText),
    flashLineRange: jest.fn(),
    isHidden: false,
    symbols: {},
    selectedLocation: { sourceId },
    onAlphabetizeClick: jest.fn(),
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = shallow(<Outline.WrappedComponent {...props} />);
  const instance = component.instance();
  return { component, props, instance };
}

describe("Outline", () => {
  afterEach(() => {
    copyToTheClipboard.mockClear();
    showMenu.mockClear();
  });

  it("renders a list of functions when properties change", async () => {
    const symbols = {
      functions: [
        makeSymbolDeclaration("my_example_function1", 21),
        makeSymbolDeclaration("my_example_function2", 22),
      ],
    };

    const { component } = render({ symbols });
    expect(component).toMatchSnapshot();
  });

  it("selects a line of code in the current file on click", async () => {
    const startLine = 12;
    const symbols = {
      functions: [makeSymbolDeclaration("my_example_function", startLine)],
    };

    const { component, props } = render({ symbols });

    const { selectLocation } = props;
    const listItem = component.find("li").first();
    listItem.simulate("click");
    expect(selectLocation).toHaveBeenCalledWith(mockcx, {
      line: startLine,
      sourceId,
    });
  });

  describe("renders outline", () => {
    describe("renders loading", () => {
      it("if symbols is not defined", () => {
        const { component } = render({
          symbols: null,
        });
        expect(component).toMatchSnapshot();
      });
    });

    it("renders ignore anonymous functions", async () => {
      const symbols = {
        functions: [
          makeSymbolDeclaration("my_example_function1", 21),
          makeSymbolDeclaration("anonymous", 25),
        ],
      };

      const { component } = render({ symbols });
      expect(component).toMatchSnapshot();
    });
    describe("renders placeholder", () => {
      it("`No File Selected` if selectedSource is not defined", async () => {
        const { component } = render({
          selectedSource: null,
        });
        expect(component).toMatchSnapshot();
      });

      it("`No functions` if all func are anonymous", async () => {
        const symbols = {
          functions: [
            makeSymbolDeclaration("anonymous", 25),
            makeSymbolDeclaration("anonymous", 30),
          ],
        };

        const { component } = render({ symbols });
        expect(component).toMatchSnapshot();
      });

      it("`No functions` if symbols has no func", async () => {
        const symbols = {
          functions: [],
        };
        const { component } = render({ symbols });
        expect(component).toMatchSnapshot();
      });
    });

    it("sorts functions alphabetically by function name", async () => {
      const symbols = {
        functions: [
          makeSymbolDeclaration("c_function", 25),
          makeSymbolDeclaration("x_function", 30),
          makeSymbolDeclaration("a_function", 70),
        ],
      };

      const { component } = render({
        symbols,
        alphabetizeOutline: true,
      });
      expect(component).toMatchSnapshot();
    });

    it("calls onAlphabetizeClick when sort button is clicked", async () => {
      const symbols = {
        functions: [makeSymbolDeclaration("example_function", 25)],
      };

      const { component, props } = render({ symbols });

      await component
        .find(".outline-footer")
        .find("button")
        .simulate("click", {});

      expect(props.onAlphabetizeClick).toHaveBeenCalled();
    });

    it("renders functions by function class", async () => {
      const symbols = {
        functions: [
          makeSymbolDeclaration("x_function", 25, 26, "x_klass"),
          makeSymbolDeclaration("a2_function", 30, 31, "a_klass"),
          makeSymbolDeclaration("a1_function", 70, 71, "a_klass"),
        ],
        classes: [
          makeSymbolDeclaration("x_klass", 24, 27),
          makeSymbolDeclaration("a_klass", 29, 72),
        ],
      };

      const { component } = render({ symbols });
      expect(component).toMatchSnapshot();
    });

    it("renders functions by function class, alphabetically", async () => {
      const symbols = {
        functions: [
          makeSymbolDeclaration("x_function", 25, 26, "x_klass"),
          makeSymbolDeclaration("a2_function", 30, 31, "a_klass"),
          makeSymbolDeclaration("a1_function", 70, 71, "a_klass"),
        ],
        classes: [
          makeSymbolDeclaration("x_klass", 24, 27),
          makeSymbolDeclaration("a_klass", 29, 72),
        ],
      };

      const { component } = render({
        symbols,
        alphabetizeOutline: true,
      });
      expect(component).toMatchSnapshot();
    });

    it("selects class on click on class headline", async () => {
      const symbols = {
        functions: [makeSymbolDeclaration("x_function", 25, 26, "x_klass")],
        classes: [makeSymbolDeclaration("x_klass", 24, 27)],
      };

      const { component, props } = render({ symbols });

      await component.find("h2").simulate("click", {});

      expect(props.selectLocation).toHaveBeenCalledWith(mockcx, {
        line: 24,
        sourceId,
      });
    });

    it("does not select an item if selectedSource is not defined", async () => {
      const { instance, props } = render({ selectedSource: null });
      await instance.selectItem({});
      expect(props.selectLocation).not.toHaveBeenCalled();
    });
  });

  describe("onContextMenu of Outline", () => {
    it("is called onContextMenu for each item", async () => {
      const event = { event: "oncontextmenu" };
      const fn = makeSymbolDeclaration("exmple_function", 2);
      const symbols = {
        functions: [fn],
      };

      const { component, instance } = render({ symbols });
      instance.onContextMenu = jest.fn(() => {});
      await component
        .find(".outline-list__element")
        .simulate("contextmenu", event);

      expect(instance.onContextMenu).toHaveBeenCalledWith(event, fn);
    });

    it("does not show menu with no selected source", async () => {
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };
      const { instance } = render({
        selectedSource: null,
      });
      await instance.onContextMenu(mockEvent, {});
      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();
      expect(showMenu).not.toHaveBeenCalled();
    });

    it("shows menu to copy func, copies to clipboard on click", async () => {
      const startLine = 12;
      const endLine = 21;
      const func = makeSymbolDeclaration(
        "my_example_function",
        startLine,
        endLine
      );
      const symbols = {
        functions: [func],
      };
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };
      const { instance, props } = render({ symbols });
      await instance.onContextMenu(mockEvent, func);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      const expectedMenuOptions = [
        {
          accesskey: "F",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-copy-function",
          label: "Copy function",
        },
      ];
      expect(props.getFunctionText).toHaveBeenCalledWith(12);
      expect(showMenu).toHaveBeenCalledWith(mockEvent, expectedMenuOptions);

      showMenu.mock.calls[0][1][0].click();
      expect(copyToTheClipboard).toHaveBeenCalledWith(mockFunctionText);
      expect(props.flashLineRange).toHaveBeenCalledWith({
        end: endLine,
        sourceId,
        start: startLine,
      });
    });
  });
});
