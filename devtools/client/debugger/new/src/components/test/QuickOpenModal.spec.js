"use strict";

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _enzyme = require("enzyme/index");

var _QuickOpenModal = require("../QuickOpenModal");

var _fuzzaldrinPlus = require("devtools/client/debugger/new/dist/vendors").vendored["fuzzaldrin-plus"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

jest.mock("fuzzaldrin-plus");

function generateModal(propOverrides, renderType = "shallow") {
  const props = _objectSpread({
    enabled: false,
    query: "",
    searchType: "sources",
    sources: [],
    tabs: [],
    selectLocation: jest.fn(),
    setQuickOpenQuery: jest.fn(),
    highlightLineRange: jest.fn(),
    clearHighlightLineRange: jest.fn(),
    closeQuickOpen: jest.fn()
  }, propOverrides);

  return {
    wrapper: renderType === "shallow" ? (0, _enzyme.shallow)(_react2.default.createElement(_QuickOpenModal.QuickOpenModal, props)) : (0, _enzyme.mount)(_react2.default.createElement(_QuickOpenModal.QuickOpenModal, props)),
    props
  };
}

describe("QuickOpenModal", () => {
  beforeEach(() => {
    _fuzzaldrinPlus.filter.mockClear();
  });
  test("Doesn't render when disabled", () => {
    const {
      wrapper
    } = generateModal();
    expect(wrapper).toMatchSnapshot();
  });
  test("Renders when enabled", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true
    });
    expect(wrapper).toMatchSnapshot();
  });
  test("Basic render with mount", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true
    }, "mount");
    expect(wrapper).toMatchSnapshot();
  });
  test("Basic render with mount & searchType = functions", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      query: "@",
      searchType: "functions",
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    expect(wrapper).toMatchSnapshot();
  });
  test("Ensure anonymous functions do not render in QuickOpenModal", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      query: "@",
      searchType: "functions",
      symbols: {
        functions: [{
          title: "anonymous"
        }, {
          title: "c"
        }, {
          title: "anonymous"
        }],
        variables: []
      }
    }, "mount");
    expect(wrapper.find("ResultList")).toHaveLength(1);
    expect(wrapper.find("li")).toHaveLength(1);
  });
  test("Basic render with mount & searchType = variables", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      query: "#",
      searchType: "variables",
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    expect(wrapper).toMatchSnapshot();
  });
  test("Basic render with mount & searchType = shortcuts", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      query: "?",
      searchType: "shortcuts",
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    expect(wrapper.find("ResultList")).toHaveLength(1);
    expect(wrapper.find("li")).toHaveLength(3);
  });
  test("updateResults on enable", () => {
    const {
      wrapper
    } = generateModal({}, "mount");
    expect(wrapper).toMatchSnapshot();
    wrapper.setProps({
      enabled: true
    });
    expect(wrapper).toMatchSnapshot();
  });
  test("basic source search", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    wrapper.find("input").simulate("change", {
      target: {
        value: "somefil"
      }
    });
    expect(_fuzzaldrinPlus.filter).toHaveBeenCalledWith([], "somefil", {
      key: "value",
      maxResults: 1000
    });
  });
  test("basic gotoSource search", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      searchType: "gotoSource",
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    wrapper.find("input").simulate("change", {
      target: {
        value: "somefil:33"
      }
    });
    expect(_fuzzaldrinPlus.filter).toHaveBeenCalledWith([], "somefil", {
      key: "value",
      maxResults: 1000
    });
  });
  test("basic symbol seach", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      searchType: "functions",
      symbols: {
        functions: [],
        variables: []
      },
      // symbol searching relies on a source being selected.
      // So we dummy out the source and the API.
      selectedSource: {
        get: jest.fn(() => true)
      }
    }, "mount");
    wrapper.find("input").simulate("change", {
      target: {
        value: "@someFunc"
      }
    });
    expect(_fuzzaldrinPlus.filter).toHaveBeenCalledWith([], "someFunc", {
      key: "value",
      maxResults: 1000
    });
  });
  test("Simple goto search query = :abc & searchType = goto", () => {
    const {
      wrapper
    } = generateModal({
      enabled: true,
      query: ":abc",
      searchType: "goto",
      symbols: {
        functions: [],
        variables: []
      }
    }, "mount");
    expect(wrapper).toMatchSnapshot();
    expect(wrapper.state().results).toEqual(null);
  });
  describe("showErrorEmoji", () => {
    it("true when no count + query", () => {
      const {
        wrapper
      } = generateModal({
        enabled: true,
        query: "test",
        searchType: ""
      }, "mount");
      expect(wrapper).toMatchSnapshot();
    });
    it("false when count + query", () => {
      const {
        wrapper
      } = generateModal({
        enabled: true,
        query: "dasdasdas"
      }, "mount");
      wrapper.setState(() => ({
        results: [1, 2]
      }));
      expect(wrapper).toMatchSnapshot();
    });
    it("false when no query", () => {
      const {
        wrapper
      } = generateModal({
        enabled: true,
        query: "",
        searchType: ""
      }, "mount");
      expect(wrapper).toMatchSnapshot();
    });
    it("false when goto numeric ':2222'", () => {
      const {
        wrapper
      } = generateModal({
        enabled: true,
        query: ":2222",
        searchType: "goto"
      }, "mount");
      expect(wrapper).toMatchSnapshot();
    });
    it("true when goto not numeric ':22k22'", () => {
      const {
        wrapper
      } = generateModal({
        enabled: true,
        query: ":22k22",
        searchType: "goto"
      }, "mount");
      expect(wrapper).toMatchSnapshot();
    });
  });
});