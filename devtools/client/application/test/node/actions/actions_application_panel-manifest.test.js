/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  MANIFEST_NO_ISSUES,
} = require("resource://devtools/client/application/test/node/fixtures/data/constants.js");

const {
  setupStore,
} = require("resource://devtools/client/application/test/node/helpers.js");

const {
  ManifestDevToolsError,
  services,
} = require("resource://devtools/client/application/src/modules/application-services.js");

const {
  FETCH_MANIFEST_FAILURE,
  FETCH_MANIFEST_START,
  FETCH_MANIFEST_SUCCESS,
} = require("resource://devtools/client/application/src/constants.js");

const {
  fetchManifest,
} = require("resource://devtools/client/application/src/actions/manifest.js");

describe("Manifest actions: fetchManifest", () => {
  it("dispatches a START - SUCCESS sequence when fetching is OK", async () => {
    const fetchManifestSpy = jest
      .spyOn(services, "fetchManifest")
      .mockResolvedValue(MANIFEST_NO_ISSUES);

    const store = setupStore({});
    await store.dispatch(fetchManifest());

    expect(store.getActions()).toEqual([
      { type: FETCH_MANIFEST_START },
      { type: FETCH_MANIFEST_SUCCESS, manifest: MANIFEST_NO_ISSUES },
    ]);

    fetchManifestSpy.mockRestore();
  });

  it("dispatches a START - FAILURE sequence when fetching fails", async () => {
    const fetchManifestSpy = jest
      .spyOn(services, "fetchManifest")
      .mockRejectedValue(new Error("lorem ipsum"));

    const store = setupStore({});
    await store.dispatch(fetchManifest());

    expect(store.getActions()).toEqual([
      { type: FETCH_MANIFEST_START },
      { type: FETCH_MANIFEST_FAILURE, error: "lorem ipsum" },
    ]);

    fetchManifestSpy.mockRestore();
  });

  it("dispatches a START - FAILURE sequence when fetching fails due to a devtools error", async () => {
    const error = new ManifestDevToolsError(":(");
    const fetchManifestSpy = jest
      .spyOn(services, "fetchManifest")
      .mockRejectedValue(error);
    const consoleErrorSpy = jest
      .spyOn(console, "error")
      .mockImplementation(() => {});

    const store = setupStore({});
    await store.dispatch(fetchManifest());

    expect(store.getActions()).toEqual([
      { type: FETCH_MANIFEST_START },
      { type: FETCH_MANIFEST_FAILURE, error: "manifest-loaded-devtools-error" },
    ]);
    expect(consoleErrorSpy).toHaveBeenCalledWith(error);

    fetchManifestSpy.mockRestore();
    consoleErrorSpy.mockRestore();
  });
});
