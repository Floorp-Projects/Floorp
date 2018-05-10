"use strict";

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _enzyme = require("enzyme/index");

var _Outline = require("../../components/PrimaryPanes/Outline");

var _Outline2 = _interopRequireDefault(_Outline);

var _testHead = require("../../utils/test-head");

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _clipboard = require("../../utils/clipboard");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

jest.mock("devtools-contextmenu", () => ({
  showMenu: jest.fn()
}));
jest.mock("../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn()
}));
const sourceId = "id";
const mockFunctionText = "mock function text";

function generateDefaults(overrides) {
  return _objectSpread({
    selectLocation: jest.genMockFunction(),
    selectedSource: {
      get: () => sourceId
    },
    getFunctionText: jest.fn().mockReturnValue(mockFunctionText),
    flashLineRange: jest.fn(),
    isHidden: false,
    symbols: {},
    selectedLocation: {
      sourceId: sourceId
    },
    onAlphabetizeClick: jest.fn()
  }, overrides);
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  const component = (0, _enzyme.shallow)(_react2.default.createElement(_Outline2.default.WrappedComponent, props));
  const instance = component.instance();
  return {
    component,
    props,
    instance
  };
}

describe("Outline", () => {
  afterEach(() => {
    _clipboard.copyToTheClipboard.mockClear();

    _devtoolsContextmenu.showMenu.mockClear();
  });
  it("renders a list of functions when properties change", async () => {
    const symbols = {
      functions: [(0, _testHead.makeSymbolDeclaration)("my_example_function1", 21), (0, _testHead.makeSymbolDeclaration)("my_example_function2", 22)]
    };
    const {
      component
    } = render({
      symbols
    });
    expect(component).toMatchSnapshot();
  });
  it("selects a line of code in the current file on click", async () => {
    const startLine = 12;
    const symbols = {
      functions: [(0, _testHead.makeSymbolDeclaration)("my_example_function", startLine)]
    };
    const {
      component,
      props
    } = render({
      symbols
    });
    const {
      selectLocation
    } = props;
    const listItem = component.find("li").first();
    listItem.simulate("click");
    expect(selectLocation).toHaveBeenCalledWith({
      line: startLine,
      sourceId
    });
  });
  describe("renders outline", () => {
    describe("renders loading", () => {
      it("if symbols is not defined", () => {
        const {
          component
        } = render({
          symbols: null
        });
        expect(component).toMatchSnapshot();
      });
      it("if symbols are loading", () => {
        const {
          component
        } = render({
          symbols: {
            loading: true
          }
        });
        expect(component).toMatchSnapshot();
      });
    });
    it("renders ignore anonymous functions", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("my_example_function1", 21), (0, _testHead.makeSymbolDeclaration)("anonymous", 25)]
      };
      const {
        component
      } = render({
        symbols
      });
      expect(component).toMatchSnapshot();
    });
    describe("renders placeholder", () => {
      it("`No File Selected` if selectedSource is not defined", async () => {
        const {
          component
        } = render({
          selectedSource: null
        });
        expect(component).toMatchSnapshot();
      });
      it("`No functions` if all func are anonymous", async () => {
        const symbols = {
          functions: [(0, _testHead.makeSymbolDeclaration)("anonymous", 25), (0, _testHead.makeSymbolDeclaration)("anonymous", 30)]
        };
        const {
          component
        } = render({
          symbols
        });
        expect(component).toMatchSnapshot();
      });
      it("`No functions` if symbols has no func", async () => {
        const symbols = {
          functions: []
        };
        const {
          component
        } = render({
          symbols
        });
        expect(component).toMatchSnapshot();
      });
    });
    it("sorts functions alphabetically by function name", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("c_function", 25), (0, _testHead.makeSymbolDeclaration)("x_function", 30), (0, _testHead.makeSymbolDeclaration)("a_function", 70)]
      };
      const {
        component
      } = render({
        symbols: symbols,
        alphabetizeOutline: true
      });
      expect(component).toMatchSnapshot();
    });
    it("calls onAlphabetizeClick when sort button is clicked", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("example_function", 25)]
      };
      const {
        component,
        props
      } = render({
        symbols
      });
      await component.find(".outline-footer").find("button").simulate("click", {});
      expect(props.onAlphabetizeClick).toHaveBeenCalled();
    });
    it("renders functions by function class", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("x_function", 25, 26, "x_klass"), (0, _testHead.makeSymbolDeclaration)("a2_function", 30, 31, "a_klass"), (0, _testHead.makeSymbolDeclaration)("a1_function", 70, 71, "a_klass")],
        classes: [(0, _testHead.makeSymbolDeclaration)("x_klass", 24, 27), (0, _testHead.makeSymbolDeclaration)("a_klass", 29, 72)]
      };
      const {
        component
      } = render({
        symbols: symbols
      });
      expect(component).toMatchSnapshot();
    });
    it("renders functions by function class, alphabetically", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("x_function", 25, 26, "x_klass"), (0, _testHead.makeSymbolDeclaration)("a2_function", 30, 31, "a_klass"), (0, _testHead.makeSymbolDeclaration)("a1_function", 70, 71, "a_klass")],
        classes: [(0, _testHead.makeSymbolDeclaration)("x_klass", 24, 27), (0, _testHead.makeSymbolDeclaration)("a_klass", 29, 72)]
      };
      const {
        component
      } = render({
        symbols: symbols,
        alphabetizeOutline: true
      });
      expect(component).toMatchSnapshot();
    });
    it("selects class on click on class headline", async () => {
      const symbols = {
        functions: [(0, _testHead.makeSymbolDeclaration)("x_function", 25, 26, "x_klass")],
        classes: [(0, _testHead.makeSymbolDeclaration)("x_klass", 24, 27)]
      };
      const {
        component,
        props
      } = render({
        symbols: symbols
      });
      await component.find("h2").simulate("click", {});
      expect(props.selectLocation).toHaveBeenCalledWith({
        line: 24,
        sourceId: sourceId
      });
    });
    it("does not select an item if selectedSource is not defined", async () => {
      const {
        instance,
        props
      } = render({
        selectedSource: null
      });
      await instance.selectItem({});
      expect(props.selectLocation).not.toHaveBeenCalled();
    });
  });
  describe("onContextMenu of Outline", () => {
    it("is called onContextMenu for each item", async () => {
      const event = {
        event: "oncontextmenu"
      };
      const fn = (0, _testHead.makeSymbolDeclaration)("exmple_function", 2);
      const symbols = {
        functions: [fn]
      };
      const {
        component,
        instance
      } = render({
        symbols
      });
      instance.onContextMenu = jest.fn(() => {});
      await component.find(".outline-list__element").simulate("contextmenu", event);
      expect(instance.onContextMenu).toHaveBeenCalledWith(event, fn);
    });
    it("does not show menu with no selected source", async () => {
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn()
      };
      const {
        instance
      } = render({
        selectedSource: null
      });
      await instance.onContextMenu(mockEvent, {});
      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();
      expect(_devtoolsContextmenu.showMenu).not.toHaveBeenCalled();
    });
    it("shows menu to copy func, copies to clipboard on click", async () => {
      const startLine = 12;
      const endLine = 21;
      const func = (0, _testHead.makeSymbolDeclaration)("my_example_function", startLine, endLine);
      const symbols = {
        functions: [func]
      };
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn()
      };
      const {
        instance,
        props
      } = render({
        symbols
      });
      await instance.onContextMenu(mockEvent, func);
      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();
      const expectedMenuOptions = [{
        accesskey: "F",
        click: expect.any(Function),
        disabled: false,
        id: "node-menu-copy-function",
        label: "Copy function"
      }];
      expect(props.getFunctionText).toHaveBeenCalledWith(12);
      expect(_devtoolsContextmenu.showMenu).toHaveBeenCalledWith(mockEvent, expectedMenuOptions);

      _devtoolsContextmenu.showMenu.mock.calls[0][1][0].click();

      expect(_clipboard.copyToTheClipboard).toHaveBeenCalledWith(mockFunctionText);
      expect(props.flashLineRange).toHaveBeenCalledWith({
        end: endLine,
        sourceId: sourceId,
        start: startLine
      });
    });
  });
});