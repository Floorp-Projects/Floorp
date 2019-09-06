/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");
// Import test helpers
const {
  flushPromises,
  setupStore,
} = require("devtools/client/application/test/components/helpers/helpers");
// Import fixtures
const {
  MANIFEST_NO_ISSUES,
} = require("devtools/client/application/test/components/fixtures/data/constants");

// Import app modules
const {
  services,
} = require("devtools/client/application/src/modules/services");

const {
  FETCH_MANIFEST_FAILURE,
  FETCH_MANIFEST_START,
  FETCH_MANIFEST_SUCCESS,
} = require("devtools/client/application/src/constants");

const ManifestLoader = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestLoader")
);

/**
 * Test for ManifestPage.js component
 */

describe("ManifestLoader", () => {
  function buildStore({ manifest, errorMessage, isLoading }) {
    const manifestState = Object.assign(
      {
        manifest: null,
        errorMessage: "",
        isLoading: false,
      },
      { manifest, errorMessage, isLoading }
    );

    return setupStore({ manifest: manifestState });
  }

  it("loads a manifest when mounted and triggers actions when loading is OK", async () => {
    const fetchManifestSpy = jest
      .spyOn(services, "fetchManifest")
      .mockResolvedValue({ manifest: MANIFEST_NO_ISSUES, errorMessage: "" });

    const store = buildStore({});

    shallow(ManifestLoader({ store })).dive();
    await flushPromises();

    expect(store.getActions()).toEqual([
      { type: FETCH_MANIFEST_START },
      { type: FETCH_MANIFEST_SUCCESS, manifest: MANIFEST_NO_ISSUES },
    ]);

    fetchManifestSpy.mockRestore();
  });

  it("loads a manifest when mounted and triggers actions when loading fails", async () => {
    const fetchManifestSpy = jest
      .spyOn(services, "fetchManifest")
      .mockResolvedValue({ manifest: null, errorMessage: "lorem ipsum" });

    const store = buildStore({});

    shallow(ManifestLoader({ store })).dive();
    await flushPromises();

    expect(store.getActions()).toEqual([
      { type: FETCH_MANIFEST_START },
      { type: FETCH_MANIFEST_FAILURE, error: "lorem ipsum" },
    ]);

    fetchManifestSpy.mockRestore();
  });

  it("renders a message when it is loading", async () => {
    const store = buildStore({ isLoading: true });
    const wrapper = shallow(ManifestLoader({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders a message when manifest has loaded OK", async () => {
    const store = buildStore({
      isLoading: false,
      manifest: MANIFEST_NO_ISSUES,
      errorMessage: "",
    });
    const wrapper = shallow(ManifestLoader({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders a message when manifest has failed to load", async () => {
    const store = buildStore({
      manifest: null,
      isLoading: false,
      errorMessage: "lorem ipsum",
    });
    const wrapper = shallow(ManifestLoader({ store })).dive();

    expect(wrapper).toMatchSnapshot();
  });
});
