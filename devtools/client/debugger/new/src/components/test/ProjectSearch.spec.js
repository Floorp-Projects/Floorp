"use strict";

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _enzyme = require("enzyme/index");

var _immutable = require("devtools/client/shared/vendor/immutable");

var _ProjectSearch = require("../ProjectSearch");

var _projectTextSearch = require("../../reducers/project-text-search");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

const hooks = {
  on: [],
  off: []
};
const shortcuts = {
  dispatch(eventName) {
    hooks.on.forEach(hook => {
      if (hook.event === eventName) {
        hook.cb();
      }
    });
    hooks.off.forEach(hook => {
      if (hook.event === eventName) {
        hook.cb();
      }
    });
  },

  on: jest.fn((event, cb) => hooks.on.push({
    event,
    cb
  })),
  off: jest.fn((event, cb) => hooks.off.push({
    event,
    cb
  }))
};
const context = {
  shortcuts
};
const testResults = (0, _immutable.List)([{
  filepath: "testFilePath1",
  matches: [{
    match: "match1",
    value: "some thing match1",
    column: 30
  }, {
    match: "match2",
    value: "some thing match2",
    column: 60
  }, {
    match: "match3",
    value: "some thing match3",
    column: 90
  }]
}, {
  filepath: "testFilePath2",
  matches: [{
    match: "match4",
    value: "some thing match4",
    column: 80
  }, {
    match: "match5",
    value: "some thing match5",
    column: 40
  }]
}]);
const testMatch = {
  match: "match1",
  value: "some thing match1",
  column: 30
};

function render(overrides = {}, mounted = false) {
  const props = _objectSpread({
    status: "DONE",
    sources: {},
    results: (0, _immutable.List)([]),
    query: "foo",
    activeSearch: "project",
    closeProjectSearch: jest.fn(),
    searchSources: jest.fn(),
    clearSearch: jest.fn(),
    updateSearchStatus: jest.fn(),
    selectLocation: jest.fn(),
    doSearchForHighlight: jest.fn()
  }, overrides);

  return mounted ? (0, _enzyme.mount)(_react2.default.createElement(_ProjectSearch.ProjectSearch, props), {
    context
  }) : (0, _enzyme.shallow)(_react2.default.createElement(_ProjectSearch.ProjectSearch, props), {
    context
  });
}

describe("ProjectSearch", () => {
  beforeEach(() => {
    context.shortcuts.on.mockClear();
    context.shortcuts.off.mockClear();
  });
  it("renders nothing when disabled", () => {
    const component = render({
      activeSearch: null
    });
    expect(component).toMatchSnapshot();
  });
  it("where <Enter> has not been pressed", () => {
    const component = render({
      query: ""
    });
    expect(component).toMatchSnapshot();
  });
  it("found no search results", () => {
    const component = render();
    expect(component).toMatchSnapshot();
  });
  it("should display loading message while search is in progress", () => {
    const component = render({
      query: "match",
      status: _projectTextSearch.statusType.fetching
    });
    expect(component).toMatchSnapshot();
  });
  it("found search results", () => {
    const component = render({
      query: "match",
      results: testResults
    }, true);
    expect(component).toMatchSnapshot();
  });
  it("turns off shortcuts on unmount", () => {
    const component = render({
      query: ""
    });
    expect(component).toMatchSnapshot();
    component.unmount();
    expect(context.shortcuts.off).toHaveBeenCalled();
  });
  it("calls inputOnChange", () => {
    const component = render({
      results: testResults
    }, true);
    component.find("SearchInput input").simulate("change", {
      target: {
        value: "bar"
      }
    });
    expect(component.state().inputValue).toEqual("bar");
  });
  it("onKeyDown Escape/Other", () => {
    const searchSources = jest.fn();
    const component = render({
      results: testResults,
      searchSources
    }, true);
    component.find("SearchInput input").simulate("keydown", {
      key: "Escape"
    });
    expect(searchSources).not.toHaveBeenCalled();
    searchSources.mockClear();
    component.find("SearchInput input").simulate("keydown", {
      key: "Other",
      stopPropagation: jest.fn()
    });
    expect(searchSources).not.toHaveBeenCalled();
  });
  it("onKeyDown Enter", () => {
    const searchSources = jest.fn();
    const component = render({
      results: testResults,
      searchSources
    }, true);
    component.find("SearchInput input").simulate("keydown", {
      key: "Enter",
      stopPropagation: jest.fn()
    });
    expect(searchSources).toHaveBeenCalledWith("foo");
  });
  it("onEnterPress shortcut no match or setExpanded", () => {
    const selectLocation = jest.fn();
    const component = render({
      results: testResults,
      selectLocation
    }, true);
    component.instance().focusedItem = {};
    shortcuts.dispatch("Enter");
    expect(selectLocation).not.toHaveBeenCalled();
  });
  it("onEnterPress shortcut match", () => {
    const selectLocation = jest.fn();
    const component = render({
      results: testResults,
      selectLocation
    }, true);
    component.instance().focusedItem = {
      match: testMatch
    };
    shortcuts.dispatch("Enter");
    expect(selectLocation).toHaveBeenCalledWith(testMatch);
  });
  it("onEnterPress shortcut setExpanded", () => {
    const selectLocation = jest.fn();
    const component = render({
      results: testResults,
      selectLocation
    }, true);
    const setExpanded = jest.fn();
    const testFile = {
      filepath: "testFilePath1",
      matches: [testMatch]
    };
    component.instance().focusedItem = {
      setExpanded,
      file: testFile,
      expanded: true
    };
    shortcuts.dispatch("Enter");
    expect(setExpanded).toHaveBeenCalledWith(testFile, false);
  });
  describe("showErrorEmoji", () => {
    it("false if not done & results", () => {
      const component = render({
        status: _projectTextSearch.statusType.fetching,
        results: testResults
      });
      expect(component).toMatchSnapshot();
    });
    it("false if not done & no results", () => {
      const component = render({
        status: _projectTextSearch.statusType.fetching
      });
      expect(component).toMatchSnapshot();
    }); // "false if done & has results"
    // is the same test as "found search results"
    // "true if done & has no results"
    // is the same test as "found no search results"
  });
});