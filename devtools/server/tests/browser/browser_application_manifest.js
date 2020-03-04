/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Testing fetching a valid manifest");
  const response = await fetchManifest("application-manifest-basic.html");

  ok(
    response.manifest && response.manifest.name == "FooApp",
    "Returns an object populated with the manifest data"
  );
});

add_task(async function() {
  info("Testing fetching an existing manifest with invalid values");
  const response = await fetchManifest("application-manifest-warnings.html");

  ok(
    response.manifest && response.manifest.moz_validation,
    "Returns an object populated with the manifest data"
  );

  const warnings = response.manifest.moz_validation;
  ok(
    warnings.length === 1 &&
      warnings[0].warn &&
      warnings[0].warn.includes("name member to be a string"),
    "The returned object contains the expected warning info"
  );
});

add_task(async function() {
  info("Testing fetching a manifest in a page that does not have one");
  const response = await fetchManifest("application-manifest-no-manifest.html");

  is(response.manifest, null, "Returns an object with a `null` manifest");
  ok(!response.errorMessage, "Does not return an error message");
});

add_task(async function() {
  info("Testing an error happening fetching a manifest");
  // the page that we are testing contains an invalid URL for the manifest
  const response = await fetchManifest(
    "application-manifest-404-manifest.html"
  );

  is(response.manifest, null, "Returns an object with a `null` manifest");
  ok(
    response.errorMessage &&
      response.errorMessage.toLowerCase().includes("404 - not found"),
    "Returns the expected error message"
  );
});

add_task(async function() {
  info("Testing a validation error when fetching a manifest with invalid JSON");
  const response = await fetchManifest(
    "application-manifest-invalid-json.html"
  );
  ok(
    response.manifest && response.manifest.moz_validation,
    "Returns an object with validation data"
  );
  const validation = response.manifest.moz_validation;
  ok(
    validation.find(x => x.error && x.type === "json"),
    "Has the expected error in the validation field"
  );
});

async function fetchManifest(filename) {
  const url = MAIN_DOMAIN + filename;
  const target = await addTabTarget(url);

  info("Initializing manifest front for tab");
  const manifestFront = await target.getFront("manifest");

  info("Fetching manifest");
  const response = await manifestFront.fetchCanonicalManifest();

  return response;
}
