/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { mount } = require("enzyme");

const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const Provider = createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const {
  setupStore,
} = require("resource://devtools/client/accessibility/test/node/helpers.js");

const {
  accessibility: { AUDIT_TYPE },
} = require("resource://devtools/shared/constants.js");
const {
  AUDIT_PROGRESS,
} = require("resource://devtools/client/accessibility/constants.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const ConnectedAuditProgressOverlayClass = require("resource://devtools/client/accessibility/components/AuditProgressOverlay.js");
const AuditProgressOverlayClass =
  ConnectedAuditProgressOverlayClass.WrappedComponent;
const AuditProgressOverlay = createFactory(ConnectedAuditProgressOverlayClass);

function testTextProgressBar(store, expectedLocalizedStringId) {
  const wrapper = mount(
    Provider(
      { store },
      LocalizationProvider({ bundles: [] }, AuditProgressOverlay())
    )
  );
  expect(wrapper.html()).toMatchSnapshot();

  const overlay = wrapper.find(AuditProgressOverlayClass);
  expect(overlay.children().length).toBe(1);

  const localized = overlay.childAt(0);
  expect(localized.prop("id")).toBe(expectedLocalizedStringId);
  expect(localized.prop("attrs")["aria-valuetext"]).toBe(true);

  const overlayText = localized.childAt(0);
  expect(overlayText.type()).toBe("span");
  expect(overlayText.prop("id")).toBe("audit-progress-container");
  expect(overlayText.prop("role")).toBe("progressbar");
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
    const wrapper = mount(
      Provider(
        { store },
        LocalizationProvider({ bundles: [] }, AuditProgressOverlay())
      )
    );
    expect(wrapper.html()).toMatchSnapshot();
    expect(wrapper.isEmptyRender()).toBe(true);
  });

  it("render auditing initializing", () => {
    const store = setupStore({
      preloadedState: { audit: { auditing: [AUDIT_TYPE.CONTRAST] } },
    });

    testTextProgressBar(store, "accessibility-progress-initializing");
  });

  it("render auditing progress", () => {
    const store = setupStore({
      preloadedState: {
        audit: {
          auditing: [AUDIT_TYPE.CONTRAST],
          progress: { total: 5, percentage: 0 },
        },
      },
    });

    const wrapper = mount(
      Provider(
        { store },
        LocalizationProvider({ bundles: [] }, AuditProgressOverlay())
      )
    );
    expect(wrapper.html()).toMatchSnapshot();

    const overlay = wrapper.find(AuditProgressOverlayClass);
    expect(overlay.children().length).toBe(1);

    const overlayContainer = overlay.childAt(0);
    expect(overlayContainer.type()).toBe("span");
    expect(overlayContainer.prop("id")).toBe("audit-progress-container");
    expect(overlayContainer.children().length).toBe(2);

    const localized = overlayContainer.childAt(0);
    expect(localized.prop("id")).toBe("accessibility-progress-progressbar");
    expect(localized.prop("$nodeCount")).toBe(5);
    expect(overlayContainer.childAt(1).type()).toBe("progress");

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
          auditing: [AUDIT_TYPE.CONTRAST],
          progress: { total: 5, percentage: 100 },
        },
      },
    });

    testTextProgressBar(store, "accessibility-progress-finishing");
  });
});
