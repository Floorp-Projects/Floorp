/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import libs
const { shallow } = require("enzyme");
const { createFactory } = require("react");

const {
  setupStore,
} = require("resource://devtools/client/application/test/node/helpers.js");
const {
  MANIFEST_SIMPLE,
} = require("resource://devtools/client/application/test/node/fixtures/data/constants.js");

const ManifestPage = createFactory(
  require("resource://devtools/client/application/src/components/manifest/ManifestPage.js")
);

/**
 * Test for ManifestPage.js component
 */

describe("ManifestPage", () => {
  function buildStoreWithManifest(manifest, isLoading = false) {
    return setupStore({
      manifest: {
        manifest,
        errorMessage: "",
        isLoading,
      },
    });
  }

  it("renders the expected snapshot when there is a manifest", () => {
    const store = buildStoreWithManifest(MANIFEST_SIMPLE);
    const wrapper = shallow(ManifestPage({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when the manifest needs to load", () => {
    const store = buildStoreWithManifest(undefined);
    const wrapper = shallow(ManifestPage({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when the manifest is loading", () => {
    const store = buildStoreWithManifest(undefined, true);
    const wrapper = shallow(ManifestPage({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });

  it("renders the expected snapshot when there is no manifest", () => {
    const store = buildStoreWithManifest(null);
    const wrapper = shallow(ManifestPage({ store })).dive();
    expect(wrapper).toMatchSnapshot();
  });
});
