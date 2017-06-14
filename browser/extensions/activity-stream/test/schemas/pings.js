const Joi = require("joi-browser");
const {MAIN_MESSAGE_TYPE, CONTENT_MESSAGE_TYPE} = require("common/Actions.jsm");

const baseKeys = {
  client_id: Joi.string().required(),
  addon_version: Joi.string().required(),
  locale: Joi.string().required(),
  session_id: Joi.string(),
  page: Joi.valid(["about:home", "about:newtab"])
};

const BasePing = Joi.object().keys(baseKeys).options({allowUnknown: true});

const UserEventPing = Joi.object().keys(Object.assign({}, baseKeys, {
  session_id: baseKeys.session_id.required(),
  page: baseKeys.page.required(),
  source: Joi.string().required(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_user_event").required(),
  metadata_source: Joi.string(),
  highlight_type: Joi.valid(["bookmarks", "recommendation", "history"]),
  recommender_type: Joi.string()
}));

// Use this to validate actions generated from Redux
const UserEventAction = Joi.object().keys({
  type: Joi.string().required(),
  data: Joi.object().keys({
    event: Joi.valid([
      "CLICK",
      "SEARCH",
      "BLOCK",
      "DELETE",
      "OPEN_NEW_WINDOW",
      "OPEN_PRIVATE_WINDOW",
      "OPEN_NEWTAB_PREFS",
      "CLOSE_NEWTAB_PREFS",
      "BOOKMARK_DELETE",
      "BOOKMARK_ADD"
    ]).required(),
    source: Joi.valid(["TOP_SITES"]),
    action_position: Joi.number().integer()
  }).required(),
  meta: Joi.object().keys({
    to: Joi.valid(MAIN_MESSAGE_TYPE).required(),
    from: Joi.valid(CONTENT_MESSAGE_TYPE).required()
  }).required()
});

const UndesiredPing = Joi.object().keys(Object.assign({}, baseKeys, {
  source: Joi.string().required(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_undesired_event").required(),
  value: Joi.number().required()
}));

const PerfPing = Joi.object().keys(Object.assign({}, baseKeys, {
  source: Joi.string(),
  event: Joi.string().required(),
  action: Joi.valid("activity_stream_performance_event").required(),
  value: Joi.number().required()
}));

const SessionPing = Joi.object().keys(Object.assign({}, baseKeys, {
  session_id: baseKeys.session_id.required(),
  page: baseKeys.page.required(),
  session_duration: Joi.number().integer().required(),
  action: Joi.valid("activity_stream_session").required()
}));

function chaiAssertions(_chai, utils) {
  const {Assertion} = _chai;

  Assertion.addMethod("validate", function(schema, schemaName) {
    const {error} = Joi.validate(this._obj, schema);
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

module.exports = {
  baseKeys,
  BasePing,
  UndesiredPing,
  UserEventPing,
  UserEventAction,
  PerfPing,
  SessionPing,
  chaiAssertions
};
