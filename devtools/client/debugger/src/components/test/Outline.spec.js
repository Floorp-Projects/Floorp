/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { shallow } from "enzyme";
import Outline from "../../components/PrimaryPanes/Outline";
import { makeSymbolDeclaration } from "../../utils/test-head";
import { showMenu } from "../../context-menu/menu";
import { copyToTheClipboard } from "../../utils/clipboard";

jest.mock("../../context-menu/menu", () => ({ showMenu: jest.fn() }));
jest.mock("../../utils/clipboard", () => ({ copyToTheClipboard: jest.fn() }));

const sourceId = "id";
const mockFunctionText = "mock function text";

function generateDefaults(overrides) {
  return {
    selectLocation: jest.fn(),
    selectedSource: { id: sourceId },
    getFunctionText: jest.fn().mockReturnValue(mockFunctionText),
    flashLineRange: jest.fn(),
    isHidden: false,
    symbols: {},
    selectedLocation: { id: sourceId },
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
    expect(selectLocation).toHaveBeenCalledWith({
      line: startLine,
      column: undefined,
      source: {
        id: sourceId,
      },
      sourceActor: null,
      sourceActorId: undefined,
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

      expect(props.selectLocation).toHaveBeenCalledWith({
        line: 24,
        column: undefined,
        source: {
          id: sourceId,
        },
        sourceActor: null,
        sourceActorId: undefined,
      });
    });

    it("does not select an item if selectedSource is not defined", async () => {
      const { instance, props } = render({ selectedSource: null });
      await instance.selectItem({});
      expect(props.selectLocation).not.toHaveBeenCalled();
    });
  });
});
