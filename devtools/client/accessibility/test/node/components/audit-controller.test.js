/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  span,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const AuditController = createFactory(
  require("resource://devtools/client/accessibility/components/AuditController.js")
);
const {
  mockAccessible,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

describe("AuditController component:", () => {
  it("dead accessible actor", () => {
    const accessibleFront = mockAccessible();
    const wrapper = mount(
      AuditController(
        {
          accessibleFront,
        },
        span()
      )
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find("span").length).toBe(1);
    expect(wrapper.find("span").first().props()).toMatchObject({
      checks: undefined,
    });

    const instance = wrapper.instance();
    expect(accessibleFront.on.mock.calls.length).toBe(1);
    expect(accessibleFront.off.mock.calls.length).toBe(1);
    expect(accessibleFront.on.mock.calls[0]).toEqual([
      "audited",
      instance.onAudited,
    ]);
    expect(accessibleFront.off.mock.calls[0]).toEqual([
      "audited",
      instance.onAudited,
    ]);
  });

  it("accessible without checks", () => {
    const accessibleFront = mockAccessible({
      actorID: "1",
    });
    const wrapper = mount(
      AuditController(
        {
          accessibleFront,
        },
        span()
      )
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(accessibleFront.audit.mock.calls.length).toBe(1);
    expect(accessibleFront.on.mock.calls.length).toBe(1);
    expect(accessibleFront.off.mock.calls.length).toBe(0);
  });

  it("accessible with checks", () => {
    const checks = { foo: "bar" };
    const accessibleFront = mockAccessible({
      actorID: "1",
      checks,
    });
    const wrapper = mount(
      AuditController(
        {
          accessibleFront,
        },
        span({ className: "child" })
      )
    );

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.state("checks")).toMatchObject(checks);
    expect(wrapper.find(".child").prop("checks")).toMatchObject(checks);
  });
});
