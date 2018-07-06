import {INITIAL_STATE, insertPinned, reducers} from "common/Reducers.jsm";
const {TopSites, App, Snippets, Prefs, Dialog, Sections} = reducers;
import {actionTypes as at} from "common/Actions.jsm";

describe("Reducers", () => {
  describe("App", () => {
    it("should return the initial state", () => {
      const nextState = App(undefined, {type: "FOO"});
      assert.equal(nextState, INITIAL_STATE.App);
    });
    it("should set initialized to true on INIT", () => {
      const nextState = App(undefined, {type: "INIT"});

      assert.propertyVal(nextState, "initialized", true);
    });
    it("should set initialized and version on INIT", () => {
      const action = {type: "INIT", data: {version: "1.2.3"}};

      const nextState = App(undefined, action);

      assert.propertyVal(nextState, "version", "1.2.3");
    });
  });
  describe("TopSites", () => {
    it("should return the initial state", () => {
      const nextState = TopSites(undefined, {type: "FOO"});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should add top sites on TOP_SITES_UPDATED", () => {
      const newRows = [{url: "foo.com"}, {url: "bar.com"}];
      const nextState = TopSites(undefined, {type: at.TOP_SITES_UPDATED, data: {links: newRows}});
      assert.equal(nextState.rows, newRows);
    });
    it("should not update state for empty action.data on TOP_SITES_UPDATED", () => {
      const nextState = TopSites(undefined, {type: at.TOP_SITES_UPDATED});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should initialize prefs on TOP_SITES_UPDATED", () => {
      const nextState = TopSites(undefined, {type: at.TOP_SITES_UPDATED, data: {links: [], pref: "foo"}});

      assert.equal(nextState.pref, "foo");
    });
    it("should pass prevState.prefs if not present in TOP_SITES_UPDATED", () => {
      const nextState = TopSites({prefs: "foo"}, {type: at.TOP_SITES_UPDATED, data: {links: []}});

      assert.equal(nextState.prefs, "foo");
    });
    it("should set editForm.site to action.data on TOP_SITES_EDIT", () => {
      const data = {index: 7};
      const nextState = TopSites(undefined, {type: at.TOP_SITES_EDIT, data});
      assert.equal(nextState.editForm.index, data.index);
    });
    it("should set editForm to null on TOP_SITES_CANCEL_EDIT", () => {
      const nextState = TopSites(undefined, {type: at.TOP_SITES_CANCEL_EDIT});
      assert.isNull(nextState.editForm);
    });
    it("should preserve the editForm.index", () => {
      const actionTypes = [at.PREVIEW_RESPONSE, at.PREVIEW_REQUEST, at.PREVIEW_REQUEST_CANCEL];
      actionTypes.forEach(type => {
        const oldState = {editForm: {index: 0, previewUrl: "foo"}};
        const action = {type, data: {url: "foo"}};
        const nextState = TopSites(oldState, action);
        assert.equal(nextState.editForm.index, 0);
      });
    });
    it("should set previewResponse on PREVIEW_RESPONSE", () => {
      const oldState = {editForm: {previewUrl: "url"}};
      const action = {type: at.PREVIEW_RESPONSE, data: {preview: "data:123", url: "url"}};
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewResponse", "data:123");
    });
    it("should return previous state if action url does not match expected", () => {
      const oldState = {editForm: {previewUrl: "foo"}};
      const action = {type: at.PREVIEW_RESPONSE, data: {url: "bar"}};
      const nextState = TopSites(oldState, action);
      assert.equal(nextState, oldState);
    });
    it("should return previous state if editForm is not set", () => {
      const actionTypes = [at.PREVIEW_RESPONSE, at.PREVIEW_REQUEST, at.PREVIEW_REQUEST_CANCEL];
      actionTypes.forEach(type => {
        const oldState = {editForm: null};
        const action = {type, data: {url: "bar"}};
        const nextState = TopSites(oldState, action);
        assert.equal(nextState, oldState, type);
      });
    });
    it("should set previewResponse to null on PREVIEW_REQUEST", () => {
      const oldState = {editForm: {previewResponse: "foo"}};
      const action = {type: at.PREVIEW_REQUEST, data: {}};
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewResponse", null);
    });
    it("should set previewUrl on PREVIEW_REQUEST", () => {
      const oldState = {editForm: {}};
      const action = {type: at.PREVIEW_REQUEST, data: {url: "bar"}};
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewUrl", "bar");
    });
    it("should add screenshots for SCREENSHOT_UPDATED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {type: at.SCREENSHOT_UPDATED, data: {url: "bar.com", screenshot: "data:123"}};
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState.rows, [{url: "foo.com"}, {url: "bar.com", screenshot: "data:123"}]);
    });
    it("should not modify rows if nothing matches the url for SCREENSHOT_UPDATED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {type: at.SCREENSHOT_UPDATED, data: {url: "baz.com", screenshot: "data:123"}};
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState, oldState);
    });
    it("should bookmark an item on PLACES_BOOKMARK_ADDED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const action = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "bar.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 1234567
        }
      };
      const nextState = TopSites(oldState, action);
      const [, newRow] = nextState.rows;
      // new row has bookmark data
      assert.equal(newRow.url, action.data.url);
      assert.equal(newRow.bookmarkGuid, action.data.bookmarkGuid);
      assert.equal(newRow.bookmarkTitle, action.data.bookmarkTitle);
      assert.equal(newRow.bookmarkDateCreated, action.data.dateAdded);

      // old row is unchanged
      assert.equal(nextState.rows[0], oldState.rows[0]);
    });
    it("should not update state for empty action.data on PLACES_BOOKMARK_ADDED", () => {
      const nextState = TopSites(undefined, {type: at.PLACES_BOOKMARK_ADDED});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should remove a bookmark on PLACES_BOOKMARK_REMOVED", () => {
      const oldState = {
        rows: [{url: "foo.com"}, {
          url: "bar.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 123456
        }]
      };
      const action = {type: at.PLACES_BOOKMARK_REMOVED, data: {url: "bar.com"}};
      const nextState = TopSites(oldState, action);
      const [, newRow] = nextState.rows;
      // new row no longer has bookmark data
      assert.equal(newRow.url, oldState.rows[1].url);
      assert.isUndefined(newRow.bookmarkGuid);
      assert.isUndefined(newRow.bookmarkTitle);
      assert.isUndefined(newRow.bookmarkDateCreated);

      // old row is unchanged
      assert.deepEqual(nextState.rows[0], oldState.rows[0]);
    });
    it("should not update state for empty action.data on PLACES_BOOKMARK_REMOVED", () => {
      const nextState = TopSites(undefined, {type: at.PLACES_BOOKMARK_REMOVED});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should update prefs on TOP_SITES_PREFS_UPDATED", () => {
      const state = TopSites({}, {type: at.TOP_SITES_PREFS_UPDATED, data: {pref: "foo"}});

      assert.equal(state.pref, "foo");
    });
    it("should not update state for empty action.data on PLACES_LINK_DELETED", () => {
      const nextState = TopSites(undefined, {type: at.PLACES_LINK_DELETED});
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should remove the site on PLACES_LINK_DELETED", () => {
      const oldState = {rows: [{url: "foo.com"}, {url: "bar.com"}]};
      const deleteAction = {type: at.PLACES_LINK_DELETED, data: {url: "foo.com"}};
      const nextState = TopSites(oldState, deleteAction);
      assert.deepEqual(nextState.rows, [{url: "bar.com"}]);
    });
  });
  describe("Prefs", () => {
    function prevState(custom = {}) {
      return Object.assign({}, INITIAL_STATE.Prefs, custom);
    }
    it("should have the correct initial state", () => {
      const state = Prefs(undefined, {});
      assert.deepEqual(state, INITIAL_STATE.Prefs);
    });
    describe("PREFS_INITIAL_VALUES", () => {
      it("should return a new object", () => {
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: {}});
        assert.notEqual(INITIAL_STATE.Prefs, state, "should not modify INITIAL_STATE");
      });
      it("should set initalized to true", () => {
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: {}});
        assert.isTrue(state.initialized);
      });
      it("should set .values", () => {
        const newValues = {foo: 1, bar: 2};
        const state = Prefs(undefined, {type: at.PREFS_INITIAL_VALUES, data: newValues});
        assert.equal(state.values, newValues);
      });
    });
    describe("PREF_CHANGED", () => {
      it("should return a new Prefs object", () => {
        const state = Prefs(undefined, {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.notEqual(INITIAL_STATE.Prefs, state, "should not modify INITIAL_STATE");
      });
      it("should set the changed pref", () => {
        const state = Prefs(prevState({foo: 1}), {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.equal(state.values.foo, 2);
      });
      it("should return a new .pref object instead of mutating", () => {
        const oldState = prevState({foo: 1});
        const state = Prefs(oldState, {type: at.PREF_CHANGED, data: {name: "foo", value: 2}});
        assert.notEqual(oldState.values, state.values);
      });
    });
  });
  describe("Dialog", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(INITIAL_STATE.Dialog, Dialog(undefined, {type: "non_existent"}));
    });
    it("should toggle visible to true on DIALOG_OPEN", () => {
      const action = {type: at.DIALOG_OPEN};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isTrue(nextState.visible);
    });
    it("should pass url data on DIALOG_OPEN", () => {
      const action = {type: at.DIALOG_OPEN, data: "some url"};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.equal(nextState.data, action.data);
    });
    it("should toggle visible to false on DIALOG_CANCEL", () => {
      const action = {type: at.DIALOG_CANCEL, data: "some url"};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isFalse(nextState.visible);
    });
    it("should return inital state on DELETE_HISTORY_URL", () => {
      const action = {type: at.DELETE_HISTORY_URL};
      const nextState = Dialog(INITIAL_STATE.Dialog, action);

      assert.deepEqual(INITIAL_STATE.Dialog, nextState);
    });
  });
  describe("Sections", () => {
    let oldState;

    beforeEach(() => {
      oldState = new Array(5).fill(null).map((v, i) => ({
        id: `foo_bar_${i}`,
        title: `Foo Bar ${i}`,
        initialized: false,
        rows: [{url: "www.foo.bar", pocket_id: 123}, {url: "www.other.url"}],
        order: i,
        type: "history"
      }));
    });

    it("should return INITIAL_STATE by default", () => {
      assert.equal(INITIAL_STATE.Sections, Sections(undefined, {type: "non_existent"}));
    });
    it("should remove the correct section on SECTION_DEREGISTER", () => {
      const newState = Sections(oldState, {type: at.SECTION_DEREGISTER, data: "foo_bar_2"});
      assert.lengthOf(newState, 4);
      const expectedNewState = oldState.splice(2, 1) && oldState;
      assert.deepEqual(newState, expectedNewState);
    });
    it("should add a section on SECTION_REGISTER if it doesn't already exist", () => {
      const action = {type: at.SECTION_REGISTER, data: {id: "foo_bar_5", title: "Foo Bar 5"}};
      const newState = Sections(oldState, action);
      assert.lengthOf(newState, 6);
      const insertedSection = newState.find(section => section.id === "foo_bar_5");
      assert.propertyVal(insertedSection, "title", action.data.title);
    });
    it("should set newSection.rows === [] if no rows are provided on SECTION_REGISTER", () => {
      const action = {type: at.SECTION_REGISTER, data: {id: "foo_bar_5", title: "Foo Bar 5"}};
      const newState = Sections(oldState, action);
      const insertedSection = newState.find(section => section.id === "foo_bar_5");
      assert.deepEqual(insertedSection.rows, []);
    });
    it("should update a section on SECTION_REGISTER if it already exists", () => {
      const NEW_TITLE = "New Title";
      const action = {type: at.SECTION_REGISTER, data: {id: "foo_bar_2", title: NEW_TITLE}};
      const newState = Sections(oldState, action);
      assert.lengthOf(newState, 5);
      const updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.ok(updatedSection && updatedSection.title === NEW_TITLE);
    });
    it("should set initialized to false on SECTION_REGISTER if there are no rows", () => {
      const NEW_TITLE = "New Title";
      const action = {type: at.SECTION_REGISTER, data: {id: "bloop", title: NEW_TITLE}};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "bloop");
      assert.propertyVal(updatedSection, "initialized", false);
    });
    it("should set initialized to true on SECTION_REGISTER if there are rows", () => {
      const NEW_TITLE = "New Title";
      const action = {type: at.SECTION_REGISTER, data: {id: "bloop", title: NEW_TITLE, rows: [{}, {}]}};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "bloop");
      assert.propertyVal(updatedSection, "initialized", true);
    });
    it("should have no effect on SECTION_UPDATE if the id doesn't exist", () => {
      const action = {type: at.SECTION_UPDATE, data: {id: "fake_id", data: "fake_data"}};
      const newState = Sections(oldState, action);
      assert.deepEqual(oldState, newState);
    });
    it("should update the section with the correct data on SECTION_UPDATE", () => {
      const FAKE_DATA = {rows: ["some", "fake", "data"], foo: "bar"};
      const action = {type: at.SECTION_UPDATE, data: Object.assign(FAKE_DATA, {id: "foo_bar_2"})};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.include(updatedSection, FAKE_DATA);
    });
    it("should set initialized to true on SECTION_UPDATE if rows is defined on action.data", () => {
      const data = {rows: [], id: "foo_bar_2"};
      const action = {type: at.SECTION_UPDATE, data};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.propertyVal(updatedSection, "initialized", true);
    });
    it("should retain pinned cards on SECTION_UPDATE", () => {
      const ROW = {id: "row"};
      let newState = Sections(oldState, {type: at.SECTION_UPDATE, data: Object.assign({rows: [ROW]}, {id: "foo_bar_2"})});
      let updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [ROW]);

      const PINNED_ROW = {id: "pinned", pinned: true};
      newState = Sections(newState, {type: at.SECTION_UPDATE, data: Object.assign({rows: [PINNED_ROW]}, {id: "foo_bar_2"})});
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [PINNED_ROW]);

      // Updating the section should retain pinned card at its index
      newState = Sections(newState, {type: at.SECTION_UPDATE, data: Object.assign({rows: [ROW]}, {id: "foo_bar_2"})});
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [PINNED_ROW, ROW]);

      // Clearing/Resetting the section should clear pinned cards
      newState = Sections(newState, {type: at.SECTION_UPDATE, data: Object.assign({rows: []}, {id: "foo_bar_2"})});
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, []);
    });
    it("should have no effect on SECTION_UPDATE_CARD if the id or url doesn't exist", () => {
      const noIdAction = {type: at.SECTION_UPDATE_CARD, data: {id: "non-existent", url: "www.foo.bar", options: {title: "New title"}}};
      const noIdState = Sections(oldState, noIdAction);
      const noUrlAction = {type: at.SECTION_UPDATE_CARD, data: {id: "foo_bar_2", url: "www.non-existent.url", options: {title: "New title"}}};
      const noUrlState = Sections(oldState, noUrlAction);
      assert.deepEqual(noIdState, oldState);
      assert.deepEqual(noUrlState, oldState);
    });
    it("should update the card with the correct data on SECTION_UPDATE_CARD", () => {
      const action = {type: at.SECTION_UPDATE_CARD, data: {id: "foo_bar_2", url: "www.other.url", options: {title: "Fake new title"}}};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "foo_bar_2");
      const updatedCard = updatedSection.rows.find(card => card.url === "www.other.url");
      assert.propertyVal(updatedCard, "title", "Fake new title");
    });
    it("should only update the cards belonging to the right section on SECTION_UPDATE_CARD", () => {
      const action = {type: at.SECTION_UPDATE_CARD, data: {id: "foo_bar_2", url: "www.other.url", options: {title: "Fake new title"}}};
      const newState = Sections(oldState, action);
      newState.forEach((section, i) => {
        if (section.id !== "foo_bar_2") {
          assert.deepEqual(section, oldState[i]);
        }
      });
    });
    it("should allow action.data to set .initialized", () => {
      const data = {rows: [], initialized: false, id: "foo_bar_2"};
      const action = {type: at.SECTION_UPDATE, data};
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.propertyVal(updatedSection, "initialized", false);
    });
    it("should dedupe based on dedupeConfigurations", () => {
      const site = {url: "foo.com"};
      const highlights = {rows: [site], id: "highlights"};
      const topstories = {rows: [site], id: "topstories"};
      const dedupeConfigurations = [{id: "topstories", dedupeFrom: ["highlights"]}];
      const action = {data: {dedupeConfigurations}, type: "SECTION_UPDATE"};
      const state = [highlights, topstories];

      const nextState = Sections(state, action);

      assert.equal(nextState.find(s => s.id === "highlights").rows.length, 1);
      assert.equal(nextState.find(s => s.id === "topstories").rows.length, 0);
    });
    it("should remove blocked and deleted urls from all rows in all sections", () => {
      const blockAction = {type: at.PLACES_LINK_BLOCKED, data: {url: "www.foo.bar"}};
      const deleteAction = {type: at.PLACES_LINK_DELETED, data: {url: "www.foo.bar"}};
      const newBlockState = Sections(oldState, blockAction);
      const newDeleteState = Sections(oldState, deleteAction);
      newBlockState.concat(newDeleteState).forEach(section => {
        assert.deepEqual(section.rows, [{url: "www.other.url"}]);
      });
    });
    it("should not update state for empty action.data on PLACES_LINK_DELETED", () => {
      const nextState = Sections(undefined, {type: at.PLACES_LINK_DELETED});
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should remove all removed pocket urls", () => {
      const removeAction = {type: at.DELETE_FROM_POCKET, data: {pocket_id: 123}};
      const newBlockState = Sections(oldState, removeAction);
      newBlockState.forEach(section => {
        assert.deepEqual(section.rows, [{url: "www.other.url"}]);
      });
    });
    it("should archive all archived pocket urls", () => {
      const removeAction = {type: at.ARCHIVE_FROM_POCKET, data: {pocket_id: 123}};
      const newBlockState = Sections(oldState, removeAction);
      newBlockState.forEach(section => {
        assert.deepEqual(section.rows, [{url: "www.other.url"}]);
      });
    });
    it("should not update state for empty action.data on PLACES_BOOKMARK_ADDED", () => {
      const nextState = Sections(undefined, {type: at.PLACES_BOOKMARK_ADDED});
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should bookmark an item when PLACES_BOOKMARK_ADDED is received", () => {
      const action = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "www.foo.bar",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 1234567
        }
      };
      const nextState = Sections(oldState, action);
      // check a section to ensure the correct url was bookmarked
      const [newRow, oldRow] = nextState[0].rows;

      // new row has bookmark data
      assert.equal(newRow.url, action.data.url);
      assert.equal(newRow.type, "bookmark");
      assert.equal(newRow.bookmarkGuid, action.data.bookmarkGuid);
      assert.equal(newRow.bookmarkTitle, action.data.bookmarkTitle);
      assert.equal(newRow.bookmarkDateCreated, action.data.dateAdded);

      // old row is unchanged
      assert.equal(oldRow, oldState[0].rows[1]);
    });
    it("should not update state for empty action.data on PLACES_BOOKMARK_REMOVED", () => {
      const nextState = Sections(undefined, {type: at.PLACES_BOOKMARK_REMOVED});
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should remove the bookmark when PLACES_BOOKMARK_REMOVED is received", () => {
      const action = {
        type: at.PLACES_BOOKMARK_REMOVED,
        data: {
          url: "www.foo.bar",
          bookmarkGuid: "bookmark123"
        }
      };
      // add some bookmark data for the first url in rows
      oldState.forEach(item => {
        item.rows[0].bookmarkGuid = "bookmark123";
        item.rows[0].bookmarkTitle = "Title for bar.com";
        item.rows[0].bookmarkDateCreated = 1234567;
        item.rows[0].type = "bookmark";
      });
      const nextState = Sections(oldState, action);
      // check a section to ensure the correct bookmark was removed
      const [newRow, oldRow] = nextState[0].rows;

      // new row isn't a bookmark
      assert.equal(newRow.url, action.data.url);
      assert.equal(newRow.type, "history");
      assert.isUndefined(newRow.bookmarkGuid);
      assert.isUndefined(newRow.bookmarkTitle);
      assert.isUndefined(newRow.bookmarkDateCreated);

      // old row is unchanged
      assert.equal(oldRow, oldState[0].rows[1]);
    });
    it("should not update state for empty action.data on PLACES_SAVED_TO_POCKET", () => {
      const nextState = Sections(undefined, {type: at.PLACES_SAVED_TO_POCKET});
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should add a pocked item on PLACES_SAVED_TO_POCKET", () => {
      const action = {
        type: at.PLACES_SAVED_TO_POCKET,
        data: {
          url: "www.foo.bar",
          pocket_id: 1234,
          title: "Title for bar.com"
        }
      };
      const nextState = Sections(oldState, action);
      // check a section to ensure the correct url was saved to pocket
      const [newRow, oldRow] = nextState[0].rows;

      // new row has pocket data
      assert.equal(newRow.url, action.data.url);
      assert.equal(newRow.type, "pocket");
      assert.equal(newRow.pocket_id, action.data.pocket_id);
      assert.equal(newRow.title, action.data.title);

      // old row is unchanged
      assert.equal(oldRow, oldState[0].rows[1]);
    });
  });
  describe("#insertPinned", () => {
    let links;

    beforeEach(() => {
      links =  new Array(12).fill(null).map((v, i) => ({url: `site${i}.com`}));
    });

    it("should place pinned links where they belong", () => {
      const pinned = [
        {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"},
        {"url": "http://example.com", "title": "example"}
      ];
      const result = insertPinned(links, pinned);
      for (let index of [0, 1]) {
        assert.equal(result[index].url, pinned[index].url);
        assert.ok(result[index].isPinned);
        assert.equal(result[index].pinIndex, index);
      }
      assert.deepEqual(result.slice(2), links);
    });
    it("should handle empty slots in the pinned list", () => {
      const pinned = [
        null,
        {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"},
        null,
        null,
        {"url": "http://example.com", "title": "example"}
      ];
      const result = insertPinned(links, pinned);
      for (let index of [1, 4]) {
        assert.equal(result[index].url, pinned[index].url);
        assert.ok(result[index].isPinned);
        assert.equal(result[index].pinIndex, index);
      }
      result.splice(4, 1);
      result.splice(1, 1);
      assert.deepEqual(result, links);
    });
    it("should handle a pinned site past the end of the list of links", () => {
      const pinned = [];
      pinned[11] = {"url": "http://github.com/mozilla/activity-stream", "title": "moz/a-s"};
      const result = insertPinned([], pinned);
      assert.equal(result[11].url, pinned[11].url);
      assert.isTrue(result[11].isPinned);
      assert.equal(result[11].pinIndex, 11);
    });
    it("should unpin previously pinned links no longer in the pinned list", () => {
      const pinned = [];
      links[2].isPinned = true;
      links[2].pinIndex = 2;
      const result = insertPinned(links, pinned);
      assert.notProperty(result[2], "isPinned");
      assert.notProperty(result[2], "pinIndex");
    });
    it("should handle a link present in both the links and pinned list", () => {
      const pinned = [links[7]];
      const result = insertPinned(links, pinned);
      assert.equal(links.length, result.length);
    });
    it("should not modify the original data", () => {
      const pinned = [{url: "http://example.com"}];

      insertPinned(links, pinned);

      assert.equal(typeof pinned[0].isPinned, "undefined");
    });
  });
  describe("Snippets", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(Snippets(undefined, {type: "some_action"}), INITIAL_STATE.Snippets);
    });
    it("should set initialized to true on a SNIPPETS_DATA action", () => {
      const state = Snippets(undefined, {type: at.SNIPPETS_DATA, data: {}});
      assert.isTrue(state.initialized);
    });
    it("should set the snippet data on a SNIPPETS_DATA action", () => {
      const data = {snippetsURL: "foo.com", version: 4};
      const state = Snippets(undefined, {type: at.SNIPPETS_DATA, data});
      assert.propertyVal(state, "snippetsURL", data.snippetsURL);
      assert.propertyVal(state, "version", data.version);
    });
    it("should reset to the initial state on a SNIPPETS_RESET action", () => {
      const state = Snippets({initalized: true, foo: "bar"}, {type: at.SNIPPETS_RESET});
      assert.equal(state, INITIAL_STATE.Snippets);
    });
    it("should set the new blocklist on SNIPPET_BLOCKED", () => {
      const state = Snippets({blockList: []}, {type: at.SNIPPET_BLOCKED, data: 1});
      assert.deepEqual(state.blockList, [1]);
    });
    it("should clear the blocklist on SNIPPETS_BLOCKLIST_CLEARED", () => {
      const state = Snippets({blockList: [1, 2]}, {type: at.SNIPPETS_BLOCKLIST_CLEARED});
      assert.deepEqual(state.blockList, []);
    });
  });
});
