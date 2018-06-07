import {CONTENT_MESSAGE_TYPE, MAIN_MESSAGE_TYPE} from "common/Actions.jsm";
import Joi from "joi-browser";

export const baseKeys = {
  // client_id will be set by PingCentre if it doesn't exist.
  client_id: Joi.string().optional(),
  addon_version: Joi.string().required(),
  locale: Joi.string().required(),
  session_id: Joi.string(),
  page: Joi.valid(["about:home", "about:newtab", "about:welcome", "unknown"]),
  user_prefs: Joi.number().integer().required()
};

export const BasePing = Joi.object().keys(baseKeys).options({allowUnknown: true});

export const eventsTelemetryExtraKeys = Joi.object().keys({
  session_id: baseKeys.session_id.required(),
  page: baseKeys.page.required(),
  addon_version: baseKeys.addon_version.required(),
  user_prefs: baseKeys.user_prefs.required(),
  action_position: Joi.string().optional()
}).options({allowUnknown: false});

export const UserEventPing = Joi.object().keys(Object.assign({}, baseKeys, {
  session_id: baseKeys.session_id.required(),
  page: baseKeys.page.required(),
  source: Joi.string().required(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_user_event").required(),
  metadata_source: Joi.string(),
  highlight_type: Joi.valid(["bookmarks", "recommendation", "history"]),
  recommender_type: Joi.string()
}));

export const UTUserEventPing = Joi.array().items(
  Joi.string().required().valid("activity_stream"),
  Joi.string().required().valid("event"),
  Joi.string().required().valid([
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
    "SAVE_TO_POCKET"
  ]),
  Joi.string().required(),
  eventsTelemetryExtraKeys
);

// Use this to validate actions generated from Redux
export const UserEventAction = Joi.object().keys({
  type: Joi.string().required(),
  data: Joi.object().keys({
    event: Joi.valid([
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
      "SUBMIT_EMAIL"
    ]).required(),
    source: Joi.valid(["TOP_SITES", "TOP_STORIES", "HIGHLIGHTS"]),
    action_position: Joi.number().integer(),
    value: Joi.object().keys({
      icon_type: Joi.valid(["tippytop", "rich_icon", "screenshot_with_icon", "screenshot", "no_image"]),
      card_type: Joi.valid(["bookmark", "trending", "pinned", "pocket"])
    })
  }).required(),
  meta: Joi.object().keys({
    to: Joi.valid(MAIN_MESSAGE_TYPE).required(),
    from: Joi.valid(CONTENT_MESSAGE_TYPE).required()
  }).required()
});

export const UndesiredPing = Joi.object().keys(Object.assign({}, baseKeys, {
  source: Joi.string().required(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_undesired_event").required(),
  value: Joi.number().required()
}));

export const TileSchema = Joi.object().keys({
  id: Joi.number().integer().required(),
  pos: Joi.number().integer()
});

export const ImpressionStatsPing = Joi.object().keys(Object.assign({}, baseKeys, {
  source: Joi.string().required(),
  impression_id: Joi.string().required(),
  client_id: Joi.valid("n/a").required(),
  session_id: Joi.valid("n/a").required(),
  action: Joi.valid("activity_stream_impression_stats").required(),
  tiles: Joi.array().items(TileSchema).required(),
  click: Joi.number().integer(),
  block: Joi.number().integer(),
  pocket: Joi.number().integer()
}));

export const PerfPing = Joi.object().keys(Object.assign({}, baseKeys, {
  source: Joi.string(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_performance_event").required(),
  value: Joi.number().required()
}));

export const SessionPing = Joi.object().keys(Object.assign({}, baseKeys, {
  session_id: baseKeys.session_id.required(),
  page: baseKeys.page.required(),
  session_duration: Joi.number().integer(),
  action: Joi.valid("activity_stream_session").required(),
  perf: Joi.object().keys({
    // How long it took in ms for data to be ready for display.
    highlights_data_late_by_ms: Joi.number().positive(),

    // Timestamp of the action perceived by the user to trigger the load
    // of this page.
    //
    // Not required at least for the error cases where the
    // observer event doesn't fire
    load_trigger_ts: Joi.number().positive()
      .notes(["server counter", "server counter alert"]),

    // What was the perceived trigger of the load action?
    //
    // Not required at least for the error cases where the observer event
    // doesn't fire
    load_trigger_type: Joi.valid(["first_window_opened",
      "menu_plus_or_keyboard", "unexpected"])
      .notes(["server counter", "server counter alert"]).required(),

    // How long it took in ms for data to be ready for display.
    topsites_data_late_by_ms: Joi.number().positive(),

    // When did the topsites element finish painting?  Note that, at least for
    // the first tab to be loaded, and maybe some others, this will be before
    // topsites has yet to receive screenshots updates from the add-on code,
    // and is therefore just showing placeholder screenshots.
    topsites_first_painted_ts: Joi.number().positive()
      .notes(["server counter", "server counter alert"]),

    // Information about the quality of TopSites images and icons.
    topsites_icon_stats: Joi.object().keys({
      custom_screenshot: Joi.number(),
      rich_icon: Joi.number(),
      screenshot: Joi.number(),
      screenshot_with_icon: Joi.number(),
      tippytop: Joi.number(),
      no_image: Joi.number()
    }),

    // The count of pinned Top Sites.
    topsites_pinned: Joi.number(),

    // When the page itself receives an event that document.visibilityState
    // == visible.
    //
    // Not required at least for the (error?) case where the
    // visibility_event doesn't fire.  (It's not clear whether this
    // can happen in practice, but if it does, we'd like to know about it).
    visibility_event_rcvd_ts: Joi.number().positive()
      .notes(["server counter", "server counter alert"]),

    // The boolean to signify whether the page is preloaded or not.
    is_preloaded: Joi.bool().required(),

    // The boolean to signify whether the page is prerendered or not.
    is_prerendered: Joi.bool().required()
  }).required()
}));

export const ASRouterEventPing = Joi.object().keys({
  client_id: Joi.string().required(),
  action: Joi.string().required(),
  impression_id: Joi.string().required(),
  source: Joi.string().required(),
  addon_version: Joi.string().required(),
  locale: Joi.string().required(),
  message_id: Joi.string().required(),
  event: Joi.string().required()
});

export const UTSessionPing = Joi.array().items(
  Joi.string().required().valid("activity_stream"),
  Joi.string().required().valid("end"),
  Joi.string().required().valid("session"),
  Joi.string().required(),
  eventsTelemetryExtraKeys
);

export function chaiAssertions(_chai, utils) {
  const {Assertion} = _chai;

  Assertion.addMethod("validate", function(schema, schemaName) {
    const {error} = Joi.validate(this._obj, schema, {allowUnknown: false});
    this.assert(
      !error,
      `Expected to be ${schemaName ? `a valid ${schemaName}` : "valid"} but there were errors: ${error}`
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
    }
  };

  Object.assign(_chai.assert, assertions);
}
