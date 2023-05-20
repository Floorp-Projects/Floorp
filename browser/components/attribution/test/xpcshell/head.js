/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AttributionCode } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);

let validAttrCodes = [
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
      content: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)%26msstoresignedin%3Dtrue",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
      content: "(not%20set)",
      msstoresignedin: true,
    },
    platforms: ["win"],
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D%26content%3D",
    parsed: { source: "google.com", medium: "organic" },
    doesNotRoundtrip: true, // `campaign=` and `=content` are dropped.
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic",
    parsed: { source: "google.com", medium: "organic" },
  },
  { code: "source%3Dgoogle.com", parsed: { source: "google.com" } },
  { code: "medium%3Dgoogle.com", parsed: { medium: "google.com" } },
  { code: "campaign%3Dgoogle.com", parsed: { campaign: "google.com" } },
  { code: "content%3Dgoogle.com", parsed: { content: "google.com" } },
  {
    code: "experiment%3Dexperimental",
    parsed: { experiment: "experimental" },
  },
  { code: "variation%3Dvaried", parsed: { variation: "varied" } },
  {
    code: "ua%3DGoogle%20Chrome%20123",
    parsed: { ua: "Google%20Chrome%20123" },
  },
  {
    code: "dltoken%3Dc18f86a3-f228-4d98-91bb-f90135c0aa9c",
    parsed: { dltoken: "c18f86a3-f228-4d98-91bb-f90135c0aa9c" },
  },
  {
    code: "dlsource%3Dsome-dl-source",
    parsed: {
      dlsource: "some-dl-source",
    },
  },
];

let invalidAttrCodes = [
  // Empty string
  "",
  // Not escaped
  "source=google.com&medium=organic&campaign=(not set)&content=(not set)",
  // Too long
  "campaign%3D" + "a".repeat(1000),
  // Unknown key name
  "source%3Dgoogle.com%26medium%3Dorganic%26large%3Dgeneticallymodified",
  // Empty key name
  "source%3Dgoogle.com%26medium%3Dorganic%26%3Dgeneticallymodified",
];

/**
 * Arrange for each test to have a unique application path for storing
 * quarantine data.
 *
 * The quarantine data is necessarily a shared system resource, managed by the
 * OS, so we need to avoid polluting it during tests.
 *
 * There are at least two ways to achieve this.  Here we use Sinon to stub the
 * relevant accessors: this has the advantage of being local and relatively easy
 * to follow.  In the App Update Service tests, an `nsIDirectoryServiceProvider`
 * is installed, which is global and much harder to extract for re-use.
 */
async function setupStubs() {
  // Local imports to avoid polluting the global namespace.
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );
  const { sinon } = ChromeUtils.importESModule(
    "resource://testing-common/Sinon.sys.mjs"
  );

  // This depends on the caller to invoke it by name.  We do try to
  // prevent the most obvious incorrect invocation, namely
  // `add_task(setupStubs)`.
  let caller = Components.stack.caller;
  const testID = caller.filename.toString().split("/").pop().split(".")[0];
  notEqual(testID, "head");

  let applicationFile = do_get_tempdir();
  applicationFile.append(testID);
  applicationFile.append("App.app");

  if (AppConstants.platform == "macosx") {
    // We're implicitly using the fact that modules are shared between importers here.
    const { MacAttribution } = ChromeUtils.importESModule(
      "resource:///modules/MacAttribution.sys.mjs"
    );
    sinon
      .stub(MacAttribution, "applicationPath")
      .get(() => applicationFile.path);
  }

  // The macOS quarantine database applies to existing paths only, so make
  // sure our mock application path exists.  This also creates the parent
  // directory for the attribution file, needed on both macOS and Windows.  We
  // don't ignore existing paths because we're inside a temporary directory:
  // this should never be invoked twice for the same test.
  await IOUtils.makeDirectory(applicationFile.path, {
    from: do_get_tempdir().path,
  });
}
