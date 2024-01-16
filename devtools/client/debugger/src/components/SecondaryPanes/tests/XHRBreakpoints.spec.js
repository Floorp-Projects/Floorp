/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { mount } from "enzyme";
import XHRBreakpoints from "../XHRBreakpoints";

const xhrMethods = [
  "ANY",
  "GET",
  "POST",
  "PUT",
  "HEAD",
  "DELETE",
  "PATCH",
  "OPTIONS",
];

// default state includes xhrBreakpoints[0] which is the checkbox that
// enables breaking on any url during an XMLHTTPRequest
function generateDefaultState(propsOverride) {
  return {
    xhrBreakpoints: [
      {
        path: "",
        method: "ANY",
        disabled: false,
        loading: false,
        text: 'URL contains ""',
      },
    ],
    enableXHRBreakpoint: () => {},
    disableXHRBreakpoint: () => {},
    updateXHRBreakpoint: () => {},
    removeXHRBreakpoint: () => {},
    setXHRBreakpoint: () => {},
    togglePauseOnAny: () => {},
    showInput: false,
    shouldPauseOnAny: false,
    onXHRAdded: () => {},
    ...propsOverride,
  };
}

function renderXHRBreakpointsComponent(propsOverride) {
  const props = generateDefaultState(propsOverride);
  const xhrBreakpointsComponent = mount(
    React.createElement(XHRBreakpoints.WrappedComponent, props)
  );
  return xhrBreakpointsComponent;
}

describe("XHR Breakpoints", function () {
  it("should render with 0 expressions passed from props", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    expect(xhrBreakpointsComponent).toMatchSnapshot();
  });

  it("should render with 8 expressions passed from props", function () {
    const allXHRBreakpointMethods = {
      xhrBreakpoints: [
        {
          path: "",
          method: "ANY",
          disabled: false,
          loading: false,
          text: 'URL contains ""',
        },
        {
          path: "this is any",
          method: "ANY",
          disabled: false,
          loading: false,
          text: 'URL contains "this is any"',
        },
        {
          path: "this is get",
          method: "GET",
          disabled: false,
          loading: false,
          text: 'URL contains "this is get"',
        },
        {
          path: "this is post",
          method: "POST",
          disabled: false,
          loading: false,
          text: 'URL contains "this is post"',
        },
        {
          path: "this is put",
          method: "PUT",
          disabled: false,
          loading: false,
          text: 'URL contains "this is put"',
        },
        {
          path: "this is head",
          method: "HEAD",
          disabled: false,
          loading: false,
          text: 'URL contains "this is head"',
        },
        {
          path: "this is delete",
          method: "DELETE",
          disabled: false,
          loading: false,
          text: 'URL contains "this is delete"',
        },
        {
          path: "this is patch",
          method: "PATCH",
          disabled: false,
          loading: false,
          text: 'URL contains "this is patch"',
        },
        {
          path: "this is options",
          method: "OPTIONS",
          disabled: false,
          loading: false,
          text: 'URL contains "this is options"',
        },
      ],
    };

    const xhrBreakpointsComponent = renderXHRBreakpointsComponent(
      allXHRBreakpointMethods
    );
    expect(xhrBreakpointsComponent).toMatchSnapshot();
  });

  it("should display xhr-input-method on click", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");

    const xhrInputContainer = xhrBreakpointsComponent.find(
      ".xhr-input-container"
    );
    expect(xhrInputContainer.hasClass("focused")).toBeTruthy();
  });

  it("should have focused and editing default to false", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    expect(xhrBreakpointsComponent.state("focused")).toBe(false);
    expect(xhrBreakpointsComponent.state("editing")).toBe(false);
  });

  it("should have state {..focused: true, editing: true} on focus", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");
    expect(xhrBreakpointsComponent.state("focused")).toBe(true);
    expect(xhrBreakpointsComponent.state("editing")).toBe(true);
  });

  // shifting focus from .xhr-input to any other element apart from
  // .xhr-input-method should unrender .xhr-input-method
  it("shifting focus should unrender XHR methods", function () {
    const propsOverride = {
      onXHRAdded: jest.fn,
      togglePauseOnAny: jest.fn,
    };
    const xhrBreakpointsComponent =
      renderXHRBreakpointsComponent(propsOverride);
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");
    let xhrInputContainer = xhrBreakpointsComponent.find(
      ".xhr-input-container"
    );
    expect(xhrInputContainer.hasClass("focused")).toBeTruthy();

    xhrBreakpointsComponent.find(".breakpoints-options").simulate("mousedown");
    expect(xhrBreakpointsComponent.state("focused")).toBe(true);
    expect(xhrBreakpointsComponent.state("editing")).toBe(true);
    expect(xhrBreakpointsComponent.state("clickedOnFormElement")).toBe(false);

    xhrBreakpointsComponent.find(".xhr-input-url").simulate("blur");
    expect(xhrBreakpointsComponent.state("focused")).toBe(false);
    expect(xhrBreakpointsComponent.state("editing")).toBe(false);
    expect(xhrBreakpointsComponent.state("clickedOnFormElement")).toBe(false);

    xhrBreakpointsComponent.find(".breakpoints-options").simulate("click");

    xhrInputContainer = xhrBreakpointsComponent.find(".xhr-input-container");
    expect(xhrInputContainer.hasClass("focused")).not.toBeTruthy();
  });

  // shifting focus from .xhr-input to .xhr-input-method
  // should not unrender .xhr-input-method
  it("shifting focus to XHR methods should not unrender", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");

    xhrBreakpointsComponent.find(".xhr-input-method").simulate("mousedown");
    expect(xhrBreakpointsComponent.state("focused")).toBe(true);
    expect(xhrBreakpointsComponent.state("editing")).toBe(false);
    expect(xhrBreakpointsComponent.state("clickedOnFormElement")).toBe(true);

    xhrBreakpointsComponent.find(".xhr-input-url").simulate("blur");
    expect(xhrBreakpointsComponent.state("focused")).toBe(true);
    expect(xhrBreakpointsComponent.state("editing")).toBe(false);
    expect(xhrBreakpointsComponent.state("clickedOnFormElement")).toBe(false);

    xhrBreakpointsComponent.find(".xhr-input-method").simulate("click");
    const xhrInputContainer = xhrBreakpointsComponent.find(
      ".xhr-input-container"
    );
    expect(xhrInputContainer.hasClass("focused")).toBeTruthy();
  });

  it("should have all 8 methods available as options", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");

    const xhrInputMethod = xhrBreakpointsComponent.find(".xhr-input-method");
    expect(xhrInputMethod.children()).toHaveLength(8);

    const actualXHRMethods = [];
    const expectedXHRMethods = xhrMethods;

    // fill the actualXHRMethods array with actual methods displayed in DOM
    for (let i = 0; i < xhrInputMethod.children().length; i++) {
      actualXHRMethods.push(xhrInputMethod.childAt(i).key());
    }

    // check each expected XHR Method to see if they match the actual methods
    expectedXHRMethods.forEach((expectedMethod, i) => {
      function compareMethods(actualMethod) {
        return expectedMethod === actualMethod;
      }
      expect(actualXHRMethods.find(compareMethods)).toBeTruthy();
    });
  });

  it("should return focus to input box after selecting a method", function () {
    const xhrBreakpointsComponent = renderXHRBreakpointsComponent();

    // focus starts off at .xhr-input
    xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");

    // click on method options and select GET
    const methodEvent = { target: { value: "GET" } };
    xhrBreakpointsComponent.find(".xhr-input-method").simulate("mousedown");
    expect(xhrBreakpointsComponent.state("inputMethod")).toBe("ANY");
    expect(xhrBreakpointsComponent.state("editing")).toBe(false);
    xhrBreakpointsComponent
      .find(".xhr-input-method")
      .simulate("change", methodEvent);

    // if state.editing changes from false to true, infer that
    // this._input.focus() is called, which shifts focus back to input box
    expect(xhrBreakpointsComponent.state("inputMethod")).toBe("GET");
    expect(xhrBreakpointsComponent.state("editing")).toBe(true);
  });

  it("should submit the URL and method when adding a breakpoint", function () {
    const setXHRBreakpointCallback = jest.fn();
    const propsOverride = {
      setXHRBreakpoint: setXHRBreakpointCallback,
      onXHRAdded: jest.fn(),
    };
    const mockEvent = {
      preventDefault: jest.fn(),
      stopPropagation: jest.fn(),
    };
    const availableXHRMethods = xhrMethods;
    expect(!!availableXHRMethods.length).toBeTruthy();

    // check each of the available methods to see whether
    // adding them as a method to a new breakpoint works as expected
    availableXHRMethods.forEach(function (method) {
      const xhrBreakpointsComponent =
        renderXHRBreakpointsComponent(propsOverride);
      xhrBreakpointsComponent.find(".xhr-input-url").simulate("focus");
      const urlValue = `${method.toLowerCase()}URLValue`;

      // simulate DOM event adding urlValue to .xhr-input
      const xhrInput = xhrBreakpointsComponent.find(".xhr-input-url");
      xhrInput.simulate("change", { target: { value: urlValue } });

      // simulate DOM event adding the input method to .xhr-input-method
      const xhrInputMethod = xhrBreakpointsComponent.find(".xhr-input-method");
      xhrInputMethod.simulate("change", { target: { value: method } });

      xhrBreakpointsComponent.find("form").simulate("submit", mockEvent);
      expect(setXHRBreakpointCallback).toHaveBeenCalledWith(urlValue, method);
    });
  });

  it("should submit the URL and method when editing a breakpoint", function () {
    const setXHRBreakpointCallback = jest.fn();
    const mockEvent = {
      preventDefault: jest.fn(),
      stopPropagation: jest.fn(),
    };
    const propsOverride = {
      updateXHRBreakpoint: setXHRBreakpointCallback,
      onXHRAdded: jest.fn(),
      xhrBreakpoints: [
        {
          path: "",
          method: "ANY",
          disabled: false,
          loading: false,
          text: 'URL contains ""',
        },
        {
          path: "this is GET",
          method: "GET",
          disabled: false,
          loading: false,
          text: 'URL contains "this is get"',
        },
      ],
    };
    const xhrBreakpointsComponent =
      renderXHRBreakpointsComponent(propsOverride);

    // load xhrBreakpoints pane with one existing xhrBreakpoint
    const existingXHRbreakpoint =
      xhrBreakpointsComponent.find(".xhr-container");
    expect(existingXHRbreakpoint).toHaveLength(1);

    // double click on existing breakpoint
    existingXHRbreakpoint.simulate("doubleclick");
    const xhrInput = xhrBreakpointsComponent.find(".xhr-input-url");
    xhrInput.simulate("focus");

    // change inputs and submit form
    const xhrInputMethod = xhrBreakpointsComponent.find(".xhr-input-method");
    xhrInput.simulate("change", { target: { value: "POSTURLValue" } });
    xhrInputMethod.simulate("change", { target: { value: "POST" } });
    xhrBreakpointsComponent.find("form").simulate("submit", mockEvent);
    expect(setXHRBreakpointCallback).toHaveBeenCalledWith(
      1,
      "POSTURLValue",
      "POST"
    );
  });
});
