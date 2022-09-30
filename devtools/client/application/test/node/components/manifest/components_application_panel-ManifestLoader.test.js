/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");
// Import test helpers
const {
  setupStore,
} = require("resource://devtools/client/application/test/node/helpers.js");
// Import fixtures
const {
  MANIFEST_NO_ISSUES,
} = require("resource://devtools/client/application/test/node/fixtures/data/constants.js");

const manifestActions = require("resource://devtools/client/application/src/actions/manifest.js");
// NOTE: we need to spy on the action before we load the component, so it gets
//       bound to the spy, not the original implementation
const fetchManifestActionSpy = jest.spyOn(manifestActions, "fetchManifest");

const ManifestLoader = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestLoader.js")
);

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

  afterAll(() => {
    fetchManifestActionSpy.mockRestore();
  });

  it("loads a manifest when mounted", async () => {
    fetchManifestActionSpy.mockReturnValue({ type: "foo" });

    const store = buildStore({});

    shallow(ManifestLoader({ store })).dive();

    expect(manifestActions.fetchManifest).toHaveBeenCalled();
    fetchManifestActionSpy.mockReset();
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
