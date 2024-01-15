/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
});

function assertValidates(validator, obj, msg) {
  const result = validator.validate(obj);
  Assert.ok(
    result.valid && result.errors.length === 0,
    `${msg} - errors = ${JSON.stringify(result.errors, undefined, 2)}`
  );
}

async function fetchSchema(uri) {
  try {
    return fetch(uri, { credentials: "omit" }).then(rsp => rsp.json());
  } catch (e) {
    throw new Error(`Could not fetch ${uri}`);
  }
}

async function schemaValidatorFor(uri, { common = false } = {}) {
  const schema = await fetchSchema(uri);
  const validator = new lazy.JsonSchema.Validator(schema);

  if (common) {
    const commonSchema = await fetchSchema(
      "resource://testing-common/FxMSCommon.schema.json"
    );
    validator.addSchema(commonSchema);
  }

  return validator;
}

async function makeValidators() {
  const experimentValidator = await schemaValidatorFor(
    "resource://activity-stream/schemas/MessagingExperiment.schema.json"
  );

  const messageValidators = {
    cfr_doorhanger: await schemaValidatorFor(
      "resource://testing-common/ExtensionDoorhanger.schema.json",
      { common: true }
    ),
    cfr_urlbar_chiclet: await schemaValidatorFor(
      "resource://testing-common/CFRUrlbarChiclet.schema.json",
      { common: true }
    ),
    infobar: await schemaValidatorFor(
      "resource://testing-common/InfoBar.schema.json",
      { common: true }
    ),
    pb_newtab: await schemaValidatorFor(
      "resource://testing-common/NewtabPromoMessage.schema.json",
      { common: true }
    ),
    spotlight: await schemaValidatorFor(
      "resource://testing-common/Spotlight.schema.json",
      { common: true }
    ),
    toast_notification: await schemaValidatorFor(
      "resource://testing-common/ToastNotification.schema.json",
      { common: true }
    ),
    toolbar_badge: await schemaValidatorFor(
      "resource://testing-common/ToolbarBadgeMessage.schema.json",
      { common: true }
    ),
    update_action: await schemaValidatorFor(
      "resource://testing-common/UpdateAction.schema.json",
      { common: true }
    ),
    whatsnew_panel_message: await schemaValidatorFor(
      "resource://testing-common/WhatsNewMessage.schema.json",
      { common: true }
    ),
    feature_callout: await schemaValidatorFor(
      // For now, Feature Callout and Spotlight share a common schema
      "resource://testing-common/Spotlight.schema.json",
      { common: true }
    ),
  };

  messageValidators.milestone_message = messageValidators.cfr_doorhanger;

  return { experimentValidator, messageValidators };
}
