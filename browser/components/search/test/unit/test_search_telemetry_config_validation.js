/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TELEMETRY_SETTINGS_KEY: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
});

/**
 * Checks to see if a value is an object or not.
 *
 * @param {*} value
 *   The value to check.
 * @returns {boolean}
 */
function isObject(value) {
  return value != null && typeof value == "object" && !Array.isArray(value);
}

/**
 * This function modifies the schema to prevent allowing additional properties
 * on objects. This is used to enforce that the schema contains everything that
 * we deliver via the search configuration.
 *
 * These checks are not enabled in-product, as we want to allow older versions
 * to keep working if we add new properties for whatever reason.
 *
 * @param {object} section
 *   The section to check to see if an additionalProperties flag should be added.
 */
function disallowAdditionalProperties(section) {
  // It is generally acceptable for new properties to be added to the
  // configuration as older builds will ignore them.
  //
  // As a result, we only check for new properties on nightly builds, and this
  // avoids us having to uplift schema changes. This also helps preserve the
  // schemas as documentation of "what was supported in this version".
  if (!AppConstants.NIGHTLY_BUILD) {
    info("Skipping additional properties validation.");
    return;
  }

  if (section.type == "object") {
    section.additionalProperties = false;
  }
  for (let value of Object.values(section)) {
    if (isObject(value)) {
      disallowAdditionalProperties(value);
    }
  }
}

add_task(async function test_search_config_validates_to_schema() {
  let schema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-telemetry-schema.json")
  );
  disallowAdditionalProperties(schema);

  let data = await RemoteSettings(TELEMETRY_SETTINGS_KEY).get();

  let validator = new JsonSchema.Validator(schema);

  for (let entry of data) {
    // Records in Remote Settings contain additional properties independent of
    // the schema. Hence, we don't want to validate their presence.
    delete entry.schema;
    delete entry.id;
    delete entry.last_modified;
    delete entry.filter_expression;

    let result = validator.validate(entry);
    let message = `Should validate ${entry.telemetryId}`;
    if (!result.valid) {
      message += `:\n${JSON.stringify(result.errors, null, 2)}`;
    }
    Assert.ok(result.valid, message);
  }
});
