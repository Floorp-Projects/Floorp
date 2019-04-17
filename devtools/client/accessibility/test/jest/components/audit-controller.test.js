/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");

const AuditController = createFactory(
  require("devtools/client/accessibility/components/AuditController"));
const { mockAccessible } = require("devtools/client/accessibility/test/jest/helpers");

describe("AuditController component:", () => {
  it("dead accessible actor", () => {
    const accessible = mockAccessible();
    const wrapper = mount(AuditController({
      accessible,
    }, span()));

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.find("span").length).toBe(1);
    expect(wrapper.find("span").first().props()).toMatchObject({
      checks: undefined,
    });

    const instance = wrapper.instance();
    expect(accessible.on.mock.calls.length).toBe(1);
    expect(accessible.off.mock.calls.length).toBe(1);
    expect(accessible.on.mock.calls[0]).toEqual(["audited", instance.onAudited]);
    expect(accessible.off.mock.calls[0]).toEqual(["audited", instance.onAudited]);
  });

  it("accessible without checks", () => {
    const accessible = mockAccessible({
      actorID: "1",
    });
    const wrapper = mount(AuditController({
      accessible,
    }, span()));

    expect(wrapper.html()).toMatchSnapshot();
    expect(accessible.audit.mock.calls.length).toBe(1);
    expect(accessible.on.mock.calls.length).toBe(1);
    expect(accessible.off.mock.calls.length).toBe(0);
  });

  it("accessible with checks", () => {
    const checks = { foo: "bar" };
    const accessible = mockAccessible({
      actorID: "1",
      checks,
    });
    const wrapper = mount(AuditController({
      accessible,
    }, span({ className: "child" })));

    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.state("checks")).toMatchObject(checks);
    expect(wrapper.find(".child").prop("checks")).toMatchObject(checks);
  });
});
