/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const { JsonSchema } = ChromeUtils.import(
  "resource://gre/modules/JsonSchema.jsm"
);

Cu.importGlobalProperties(["fetch"]);

let MESSAGING_EXPERIMENT_SCHEMA;
let CFR_SCHEMA;
let UPDATE_ACTION_SCHEMA;
let WHATS_NEW_SCHEMA;
let SPOTLIGHT_SCHEMA;
let PB_NEWTAB_SCHEMA;

add_setup(async function setup() {
  function fetchSchema(uri) {
    return fetch(uri, { credentials: "omit" }).then(rsp => rsp.json());
  }

  MESSAGING_EXPERIMENT_SCHEMA = await fetchSchema(
    "resource://activity-stream/schemas/MessagingExperiment.schema.json"
  );
  CFR_SCHEMA = await fetchSchema(
    "resource://testing-common/ExtensionDoorhanger.schema.json"
  );
  UPDATE_ACTION_SCHEMA = await fetchSchema(
    "resource://testing-common/UpdateAction.schema.json"
  );
  WHATS_NEW_SCHEMA = await fetchSchema(
    "resource://testing-common/WhatsNewMessage.schema.json"
  );
  SPOTLIGHT_SCHEMA = await fetchSchema(
    "resource://testing-common/Spotlight.schema.json"
  );
  PB_NEWTAB_SCHEMA = await fetchSchema(
    "resource://testing-common/NewtabPromoMessage.schema.json"
  );
});

function assertSchema(obj, schema, log) {
  Assert.deepEqual(
    JsonSchema.validate(obj, schema),
    { valid: true, errors: [] },
    `${log} (${schema.title} schema)`
  );

  Assert.deepEqual(
    JsonSchema.validate(obj, MESSAGING_EXPERIMENT_SCHEMA),
    { valid: true, errors: [] },
    `${log} (MessagingExperiment schema)`
  );
}

add_task(async function test_PanelTestProvider() {
  const messages = await PanelTestProvider.getMessages();

  // Careful: when changing this number make sure the new messages also go
  // through schema validation.
  Assert.strictEqual(
    messages.length,
    16,
    "PanelTestProvider should have the correct number of messages"
  );

  for (const [i, msg] of messages
    .filter(m => ["cfr_doorhanger", "milestone_message"].includes(m.template))
    .entries()) {
    assertSchema(msg, CFR_SCHEMA, `cfr message ${msg.id ?? i} is valid`);
  }

  for (const [i, msg] of messages
    .filter(m => m.template === "update_action")
    .entries()) {
    assertSchema(
      msg,
      UPDATE_ACTION_SCHEMA,
      `update_action message ${msg.id ?? i} is valid`
    );
  }

  for (const [i, msg] of messages
    .filter(m => m.template === "whatsnew_panel_message")
    .entries()) {
    assertSchema(
      msg,
      WHATS_NEW_SCHEMA,
      `whatsnew_panel_message message ${msg.id ?? i} is valid`
    );
  }

  for (const [i, msg] of messages
    .filter(m => m.template === "spotlight")
    .entries()) {
    assertSchema(
      msg,
      SPOTLIGHT_SCHEMA,
      `spotlight message ${msg.id ?? i} is valid`
    );
  }

  for (const [i, msg] of messages
    .filter(m => m.template === "pb_newtab")
    .entries()) {
    assertSchema(
      msg,
      PB_NEWTAB_SCHEMA,
      `pb_newtab message ${msg.id ?? i} is valid`
    );
  }

  Assert.strictEqual(
    messages.filter(m => m.template === "pb_newtab").length,
    1,
    "There is one pb_newtab message"
  );
});
