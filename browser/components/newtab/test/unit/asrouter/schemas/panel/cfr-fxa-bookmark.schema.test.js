import schema from "content-src/asrouter/schemas/panel/cfr-fxa-bookmark.schema.json";

const DEFAULT_CONTENT = {
  "title": "Sync your bookmarks everywhere",
  "text": "Great find! Now don't be left without this bookmark.",
  "cta": "Sync bookmarks now",
  "info_icon": {
    "tooltiptext": "Learn more",
  },
};

const L10N_CONTENT = {
  "title": {string_id: "cfr-bookmark-title"},
  "text": {string_id: "cfr-bookmark-body"},
  "cta": {string_id: "cfr-bookmark-link-text"},
  "info_icon": {
    "tooltiptext": {string_id: "cfr-bookmark-tooltip-text"},
  },
};

describe("CFR FxA Message Schema", () => {
  it("should validate DEFAULT_CONTENT", () => {
    assert.jsonSchema(DEFAULT_CONTENT, schema);
  });
  it("should validate L10N_CONTENT", () => {
    assert.jsonSchema(L10N_CONTENT, schema);
  });
});
