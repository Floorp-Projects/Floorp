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

// Import app modules
const {
  services,
} = require("devtools/client/application/src/modules/services");
const {
  UPDATE_MANIFEST,
} = require("devtools/client/application/src/constants");

const ManifestLoader = createFactory(
  require("devtools/client/application/src/components/manifest/ManifestLoader")
);

/**
 * Test for ManifestPage.js component
 */

describe("ManifestLoader", () => {
  const store = setupStore({
    preloadedState: {
      manifest: {
        manifest: null,
        errorMesssage: "",
      },
    },
  });

  let fetchManifestSpy;
  let dispatchSpy;

  beforeAll(() => {
    fetchManifestSpy = jest.spyOn(services, "fetchManifest");
    dispatchSpy = jest.spyOn(store, "dispatch");
  });

  beforeEach(() => {
    fetchManifestSpy.mockReset();
    dispatchSpy.mockReset();
  });

  it("triggers loading a manifest and renders 'Loading' message", () => {
    fetchManifestSpy.mockImplementation(() => new Promise(() => {}));
    const wrapper = shallow(ManifestLoader({ store })).dive();

    expect(wrapper.state("hasLoaded")).toBe(false);
    expect(wrapper.state("hasManifest")).toBe(false);
    expect(wrapper.state("error")).toBe("");
    expect(services.fetchManifest).toHaveBeenCalled();
    expect(wrapper).toMatchSnapshot();
  });

  it("updates the state and renders a message when manifest has loaded OK", async () => {
    fetchManifestSpy.mockResolvedValue({
      manifest: { name: "foo" },
      errorMessage: "",
    });

    const wrapper = shallow(ManifestLoader({ store })).dive();
    await flushPromises();

    expect(wrapper.state("hasLoaded")).toBe(true);
    expect(wrapper.state("hasManifest")).toBe(true);
    expect(wrapper.state("error")).toBe("");
    expect(store.dispatch).toHaveBeenCalledWith({
      type: UPDATE_MANIFEST,
      manifest: { name: "foo" },
      errorMessage: "",
    });
    expect(wrapper).toMatchSnapshot();
  });

  it("updates the state and renders a message when manifest has failed to load", async () => {
    fetchManifestSpy.mockResolvedValue({
      manifest: null,
      errorMessage: "lorem ipsum",
    });

    const wrapper = shallow(ManifestLoader({ store })).dive();
    await flushPromises();

    expect(wrapper.state("hasLoaded")).toBe(true);
    expect(wrapper.state("hasManifest")).toBe(false);
    expect(wrapper.state("error")).toBe("lorem ipsum");
    expect(store.dispatch).toHaveBeenCalledWith({
      type: UPDATE_MANIFEST,
      manifest: null,
      errorMessage: "lorem ipsum",
    });
    expect(wrapper).toMatchSnapshot();
  });

  it("updates the state and renders a message when page has no manifest", async () => {
    fetchManifestSpy.mockResolvedValue({
      manifest: null,
      errorMessage: "",
    });

    const wrapper = shallow(ManifestLoader({ store })).dive();
    await flushPromises();

    expect(wrapper.state("hasLoaded")).toBe(true);
    expect(wrapper.state("hasManifest")).toBe(false);
    expect(wrapper.state("error")).toBe("");
    expect(store.dispatch).toHaveBeenCalledWith({
      type: UPDATE_MANIFEST,
      manifest: null,
      errorMessage: "",
    });
    expect(wrapper).toMatchSnapshot();
  });

  afterAll(() => {
    fetchManifestSpy.mockRestore();
    dispatchSpy.mockRestore();
  });
});
