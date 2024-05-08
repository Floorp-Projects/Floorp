/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let schemas = [
  ["search-telemetry-v2-schema.json", "search-telemetry-v2-ui-schema.json"],
];

function checkUISchemaValid(configSchema, uiSchema) {
  for (let key of Object.keys(configSchema.properties)) {
    Assert.ok(
      uiSchema["ui:order"].includes(key),
      `Should have ${key} listed at the top-level of the ui schema`
    );
  }
}

add_task(async function test_ui_schemas_valid() {
  for (let [schema, uiSchema] of schemas) {
    info(`Validating ${uiSchema} has every top-level from ${schema}`);
    let schemaData = await IOUtils.readJSON(
      PathUtils.join(do_get_cwd().path, schema)
    );
    let uiSchemaData = await IOUtils.readJSON(
      PathUtils.join(do_get_cwd().path, uiSchema)
    );

    checkUISchemaValid(schemaData, uiSchemaData);
  }
});
