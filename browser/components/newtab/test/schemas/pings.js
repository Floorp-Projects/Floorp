import {
  CONTENT_MESSAGE_TYPE,
  MAIN_MESSAGE_TYPE,
} from "common/Actions.sys.mjs";
import Joi from "joi-browser";

export const baseKeys = {
  client_id: Joi.string().optional(),
  addon_version: Joi.string().required(),
  locale: Joi.string().required(),
  session_id: Joi.string(),
  page: Joi.valid([
    "about:home",
    "about:newtab",
    "about:welcome",
    "both",
    "unknown",
  ]),
  user_prefs: Joi.number().integer().required(),
};

export const eventsTelemetryExtraKeys = Joi.object()
  .keys({
    session_id: baseKeys.session_id.required(),
    page: baseKeys.page.required(),
    addon_version: baseKeys.addon_version.required(),
    user_prefs: baseKeys.user_prefs.required(),
    action_position: Joi.string().optional(),
  })
  .options({ allowUnknown: false });

export const UTUserEventPing = Joi.array().items(
  Joi.string().required().valid("activity_stream"),
  Joi.string().required().valid("event"),
  Joi.string()
    .required()
    .valid([
      "CLICK",
      "SEARCH",
      "BLOCK",
      "DELETE",
      "DELETE_CONFIRM",
      "DIALOG_CANCEL",
      "DIALOG_OPEN",
      "OPEN_NEW_WINDOW",
      "OPEN_PRIVATE_WINDOW",
      "OPEN_NEWTAB_PREFS",
      "CLOSE_NEWTAB_PREFS",
      "BOOKMARK_DELETE",
      "BOOKMARK_ADD",
      "PIN",
      "UNPIN",
      "SAVE_TO_POCKET",
    ]),
  Joi.string().required(),
  eventsTelemetryExtraKeys
);

// Use this to validate actions generated from Redux
export const UserEventAction = Joi.object().keys({
  type: Joi.string().required(),
  data: Joi.object()
    .keys({
      event: Joi.valid([
        "CLICK",
        "SEARCH",
        "SEARCH_HANDOFF",
        "BLOCK",
        "DELETE",
        "DELETE_CONFIRM",
        "DIALOG_CANCEL",
        "DIALOG_OPEN",
        "OPEN_NEW_WINDOW",
        "OPEN_PRIVATE_WINDOW",
        "OPEN_NEWTAB_PREFS",
        "CLOSE_NEWTAB_PREFS",
        "BOOKMARK_DELETE",
        "BOOKMARK_ADD",
        "PIN",
        "PREVIEW_REQUEST",
        "UNPIN",
        "SAVE_TO_POCKET",
        "MENU_MOVE_UP",
        "MENU_MOVE_DOWN",
        "SCREENSHOT_REQUEST",
        "MENU_REMOVE",
        "MENU_COLLAPSE",
        "MENU_EXPAND",
        "MENU_MANAGE",
        "MENU_ADD_TOPSITE",
        "MENU_PRIVACY_NOTICE",
        "DELETE_FROM_POCKET",
        "ARCHIVE_FROM_POCKET",
        "SKIPPED_SIGNIN",
        "SUBMIT_EMAIL",
        "SUBMIT_SIGNIN",
        "SHOW_PRIVACY_INFO",
        "CLICK_PRIVACY_INFO",
      ]).required(),
      source: Joi.valid(["TOP_SITES", "TOP_STORIES", "HIGHLIGHTS"]),
      action_position: Joi.number().integer(),
      value: Joi.object().keys({
        icon_type: Joi.valid([
          "tippytop",
          "rich_icon",
          "screenshot_with_icon",
          "screenshot",
          "no_image",
          "custom_screenshot",
        ]),
        card_type: Joi.valid([
          "bookmark",
          "trending",
          "pinned",
          "pocket",
          "search",
          "spoc",
          "organic",
        ]),
        search_vendor: Joi.valid(["google", "amazon"]),
        has_flow_params: Joi.bool(),
      }),
    })
    .required(),
  meta: Joi.object()
    .keys({
      to: Joi.valid(MAIN_MESSAGE_TYPE).required(),
      from: Joi.valid(CONTENT_MESSAGE_TYPE).required(),
    })
    .required(),
});

export const TileSchema = Joi.object().keys({
  id: Joi.number().integer().required(),
  pos: Joi.number().integer(),
});

export const UTSessionPing = Joi.array().items(
  Joi.string().required().valid("activity_stream"),
  Joi.string().required().valid("end"),
  Joi.string().required().valid("session"),
  Joi.string().required(),
  eventsTelemetryExtraKeys
);

export function chaiAssertions(_chai, utils) {
  const { Assertion } = _chai;

  Assertion.addMethod("validate", function (schema, schemaName) {
    const { error } = Joi.validate(this._obj, schema, { allowUnknown: false });
    this.assert(
      !error,
      `Expected to be ${
        schemaName ? `a valid ${schemaName}` : "valid"
      } but there were errors: ${error}`
    );
  });

  const assertions = {
    /**
     * assert.validate - Validates an item given a Joi schema
     *
     * @param  {any} actual The item to validate
     * @param  {obj} schema A Joi schema
     */
    validate(actual, schema, schemaName) {
      new Assertion(actual).validate(schema, schemaName);
    },

    /**
     * isUserEventAction - Passes if the item is a valid UserEvent action
     *
     * @param  {any} actual The item to validate
     */
    isUserEventAction(actual) {
      new Assertion(actual).validate(UserEventAction, "UserEventAction");
    },
  };

  Object.assign(_chai.assert, assertions);
}
