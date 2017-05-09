const Joi = require("joi-browser");

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

function assertMatchesSchema(ping, schema) {
  assert.isNull(Joi.validate(ping, schema).error);
}

module.exports = {
  baseKeys,
  BasePing,
  UndesiredPing,
  UserEventPing,
  PerfPing,
  SessionPing,
  assertMatchesSchema
};
