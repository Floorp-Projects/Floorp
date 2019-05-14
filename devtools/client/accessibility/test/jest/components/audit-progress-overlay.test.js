/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const { createFactory } = require("devtools/client/shared/vendor/react");
const Provider = createFactory(require("devtools/client/shared/vendor/react-redux").Provider);
const { setupStore } = require("devtools/client/accessibility/test/jest/helpers");

const { accessibility: { AUDIT_TYPE } } = require("devtools/shared/constants");
const { AUDIT_PROGRESS } = require("devtools/client/accessibility/constants");

const ConnectedAuditProgressOverlayClass =
  require("devtools/client/accessibility/components/AuditProgressOverlay");
const AuditProgressOverlayClass = ConnectedAuditProgressOverlayClass.WrappedComponent;
const AuditProgressOverlay = createFactory(ConnectedAuditProgressOverlayClass);

function testTextProgressBar(store, expectedText) {
  const wrapper = mount(Provider({ store }, AuditProgressOverlay()));
  expect(wrapper.html()).toMatchSnapshot();

  const overlay = wrapper.find(AuditProgressOverlayClass);
  expect(overlay.children().length).toBe(1);

  const overlayText = overlay.childAt(0);
  expect(overlayText.type()).toBe("span");
  expect(overlayText.prop("id")).toBe("audit-progress-container");
  expect(overlayText.prop("role")).toBe("progressbar");
  expect(overlayText.prop("aria-valuetext")).toBe(expectedText);
  expect(overlayText.text()).toBe(expectedText);
}

function testProgress(wrapper, percentage) {
  const progress = wrapper.find("progress");
  expect(progress.prop("max")).toBe(100);
  expect(progress.prop("value")).toBe(percentage);
  expect(progress.hasClass("audit-progress-progressbar")).toBe(true);
  expect(progress.prop("aria-labelledby")).toBe("audit-progress-container");
}

describe("AuditProgressOverlay component:", () => {
  it("render not auditing", () => {
    const store = setupStore();
    const wrapper = mount(Provider({ store }, AuditProgressOverlay()));
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("render auditing initializing", () => {
    const store = setupStore({
      preloadedState: { audit: { auditing: AUDIT_TYPE.CONTRAST } },
    });

    testTextProgressBar(store, "accessibility.progress.initializing");
  });

  it("render auditing progress", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          auditing: AUDIT_TYPE.CONTRAST,
          progress: { total: 5, percentage: 0 },
        },
      },
    });

    const wrapper = mount(Provider({ store }, AuditProgressOverlay()));
    expect(wrapper.html()).toMatchSnapshot();

    const overlay = wrapper.find(AuditProgressOverlayClass);
    expect(overlay.children().length).toBe(1);

    const overlayContainer = overlay.childAt(0);
    expect(overlayContainer.type()).toBe("span");
    expect(overlayContainer.prop("id")).toBe("audit-progress-container");
    expect(overlayContainer.children().length).toBe(1);

    expect(overlayContainer.text()).toBe("accessibility.progress.progressbar");
    expect(overlayContainer.childAt(0).type()).toBe("progress");

    testProgress(wrapper, 0);

    store.dispatch({
      type: AUDIT_PROGRESS,
      progress: { total: 5, percentage: 50 },
    });
    wrapper.update();

    expect(wrapper.html()).toMatchSnapshot();
    testProgress(wrapper, 50);

    store.dispatch({
      type: AUDIT_PROGRESS,
      progress: { total: 5, percentage: 75 },
    });
    wrapper.update();

    expect(wrapper.html()).toMatchSnapshot();
    testProgress(wrapper, 75);
  });

  it("render auditing finishing up", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          auditing: AUDIT_TYPE.CONTRAST,
          progress: { total: 5, percentage: 100 },
        },
      },
    });

    testTextProgressBar(store, "accessibility.progress.finishing");
  });
});
