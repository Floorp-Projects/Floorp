/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow } = require("enzyme");

const {
  REPS,
  getRep,
} = require("devtools/client/shared/components/reps/reps/rep");

const { Accessor, Rep } = REPS;

const stubs = require("devtools/client/shared/components/test/node/stubs/reps/accessor");

describe("Accessor - getter", () => {
  const object = stubs.get("getter");

  it("Rep correctly selects Accessor Rep", () => {
    expect(getRep(object)).toBe(Accessor.rep);
  });

  it("Accessor rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({ object, shouldRenderTooltip: true })
    );
    expect(renderedComponent.text()).toEqual("Getter");
    expect(renderedComponent.prop("title")).toEqual("Getter");

    const node = renderedComponent.find(".jump-definition");
    expect(node.exists()).toBeFalsy();
  });
});

describe("Accessor - setter", () => {
  const object = stubs.get("setter");

  it("Rep correctly selects Accessor Rep", () => {
    expect(getRep(object)).toBe(Accessor.rep);
  });

  it("Accessor rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({ object, shouldRenderTooltip: true })
    );
    expect(renderedComponent.text()).toEqual("Setter");
    expect(renderedComponent.prop("title")).toEqual("Setter");

    const node = renderedComponent.find(".jump-definition");
    expect(node.exists()).toBeFalsy();
  });
});

describe("Accessor - getter & setter", () => {
  const object = stubs.get("getter setter");

  it("Rep correctly selects Accessor Rep", () => {
    expect(getRep(object)).toBe(Accessor.rep);
  });

  it("Accessor rep has expected text content", () => {
    const renderedComponent = shallow(
      Rep({ object, shouldRenderTooltip: true })
    );
    expect(renderedComponent.text()).toEqual("Getter & Setter");
    expect(renderedComponent.prop("title")).toEqual("Getter & Setter");

    const node = renderedComponent.find(".jump-definition");
    expect(node.exists()).toBeFalsy();
  });
});

describe("Accessor - Invoke getter", () => {
  it("renders an icon for getter with onInvokeGetterButtonClick", () => {
    const onInvokeGetterButtonClick = jest.fn();
    const object = stubs.get("getter");
    const renderedComponent = shallow(
      Rep({ object, onInvokeGetterButtonClick })
    );

    const node = renderedComponent.find(".invoke-getter");
    node.simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });
    expect(node.prop("title")).toEqual("Invoke getter");
    expect(node.exists()).toBeTruthy();
    expect(onInvokeGetterButtonClick.mock.calls).toHaveLength(1);
  });

  it("does not render an icon for a setter only", () => {
    const onInvokeGetterButtonClick = jest.fn();
    const object = stubs.get("setter");
    const renderedComponent = shallow(
      Rep({ object, onInvokeGetterButtonClick })
    );
    expect(renderedComponent.text()).toEqual("Setter");

    const node = renderedComponent.find(".jump-definition");
    expect(node.exists()).toBeFalsy();
  });

  it("renders an icon for getter/setter with onInvokeGetterButtonClick", () => {
    const onInvokeGetterButtonClick = jest.fn();
    const object = stubs.get("getter setter");
    const renderedComponent = shallow(
      Rep({ object, onInvokeGetterButtonClick })
    );

    const node = renderedComponent.find(".invoke-getter");
    node.simulate("click", {
      type: "click",
      stopPropagation: () => {},
    });

    expect(node.exists()).toBeTruthy();
    expect(onInvokeGetterButtonClick.mock.calls).toHaveLength(1);
  });

  it("does not render an icon when the object has an evaluation", () => {
    const onInvokeGetterButtonClick = jest.fn();
    const object = stubs.get("getter");
    const renderedComponent = shallow(
      Rep({
        object,
        onInvokeGetterButtonClick,
        evaluation: { getterValue: "hello" },
      })
    );
    expect(renderedComponent.text()).toMatchSnapshot();

    const node = renderedComponent.find(".invoke-getter");
    expect(node.exists()).toBeFalsy();
  });
});
