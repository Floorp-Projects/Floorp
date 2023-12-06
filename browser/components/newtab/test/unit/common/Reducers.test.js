import { INITIAL_STATE, insertPinned, reducers } from "common/Reducers.sys.mjs";
const {
  TopSites,
  App,
  Prefs,
  Dialog,
  Sections,
  Pocket,
  Personalization,
  DiscoveryStream,
  Search,
  ASRouter,
} = reducers;
import { actionTypes as at } from "common/Actions.sys.mjs";

describe("Reducers", () => {
  describe("App", () => {
    it("should return the initial state", () => {
      const nextState = App(undefined, { type: "FOO" });
      assert.equal(nextState, INITIAL_STATE.App);
    });
    it("should set initialized to true on INIT", () => {
      const nextState = App(undefined, { type: "INIT" });

      assert.propertyVal(nextState, "initialized", true);
    });
  });
  describe("TopSites", () => {
    it("should return the initial state", () => {
      const nextState = TopSites(undefined, { type: "FOO" });
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should add top sites on TOP_SITES_UPDATED", () => {
      const newRows = [{ url: "foo.com" }, { url: "bar.com" }];
      const nextState = TopSites(undefined, {
        type: at.TOP_SITES_UPDATED,
        data: { links: newRows },
      });
      assert.equal(nextState.rows, newRows);
    });
    it("should not update state for empty action.data on TOP_SITES_UPDATED", () => {
      const nextState = TopSites(undefined, { type: at.TOP_SITES_UPDATED });
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should initialize prefs on TOP_SITES_UPDATED", () => {
      const nextState = TopSites(undefined, {
        type: at.TOP_SITES_UPDATED,
        data: { links: [], pref: "foo" },
      });

      assert.equal(nextState.pref, "foo");
    });
    it("should pass prevState.prefs if not present in TOP_SITES_UPDATED", () => {
      const nextState = TopSites(
        { prefs: "foo" },
        { type: at.TOP_SITES_UPDATED, data: { links: [] } }
      );

      assert.equal(nextState.prefs, "foo");
    });
    it("should set editForm.site to action.data on TOP_SITES_EDIT", () => {
      const data = { index: 7 };
      const nextState = TopSites(undefined, { type: at.TOP_SITES_EDIT, data });
      assert.equal(nextState.editForm.index, data.index);
    });
    it("should set editForm to null on TOP_SITES_CANCEL_EDIT", () => {
      const nextState = TopSites(undefined, { type: at.TOP_SITES_CANCEL_EDIT });
      assert.isNull(nextState.editForm);
    });
    it("should preserve the editForm.index", () => {
      const actionTypes = [
        at.PREVIEW_RESPONSE,
        at.PREVIEW_REQUEST,
        at.PREVIEW_REQUEST_CANCEL,
      ];
      actionTypes.forEach(type => {
        const oldState = { editForm: { index: 0, previewUrl: "foo" } };
        const action = { type, data: { url: "foo" } };
        const nextState = TopSites(oldState, action);
        assert.equal(nextState.editForm.index, 0);
      });
    });
    it("should set previewResponse on PREVIEW_RESPONSE", () => {
      const oldState = { editForm: { previewUrl: "url" } };
      const action = {
        type: at.PREVIEW_RESPONSE,
        data: { preview: "data:123", url: "url" },
      };
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewResponse", "data:123");
    });
    it("should return previous state if action url does not match expected", () => {
      const oldState = { editForm: { previewUrl: "foo" } };
      const action = { type: at.PREVIEW_RESPONSE, data: { url: "bar" } };
      const nextState = TopSites(oldState, action);
      assert.equal(nextState, oldState);
    });
    it("should return previous state if editForm is not set", () => {
      const actionTypes = [
        at.PREVIEW_RESPONSE,
        at.PREVIEW_REQUEST,
        at.PREVIEW_REQUEST_CANCEL,
      ];
      actionTypes.forEach(type => {
        const oldState = { editForm: null };
        const action = { type, data: { url: "bar" } };
        const nextState = TopSites(oldState, action);
        assert.equal(nextState, oldState, type);
      });
    });
    it("should set previewResponse to null on PREVIEW_REQUEST", () => {
      const oldState = { editForm: { previewResponse: "foo" } };
      const action = { type: at.PREVIEW_REQUEST, data: {} };
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewResponse", null);
    });
    it("should set previewUrl on PREVIEW_REQUEST", () => {
      const oldState = { editForm: {} };
      const action = { type: at.PREVIEW_REQUEST, data: { url: "bar" } };
      const nextState = TopSites(oldState, action);
      assert.propertyVal(nextState.editForm, "previewUrl", "bar");
    });
    it("should add screenshots for SCREENSHOT_UPDATED", () => {
      const oldState = { rows: [{ url: "foo.com" }, { url: "bar.com" }] };
      const action = {
        type: at.SCREENSHOT_UPDATED,
        data: { url: "bar.com", screenshot: "data:123" },
      };
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState.rows, [
        { url: "foo.com" },
        { url: "bar.com", screenshot: "data:123" },
      ]);
    });
    it("should not modify rows if nothing matches the url for SCREENSHOT_UPDATED", () => {
      const oldState = { rows: [{ url: "foo.com" }, { url: "bar.com" }] };
      const action = {
        type: at.SCREENSHOT_UPDATED,
        data: { url: "baz.com", screenshot: "data:123" },
      };
      const nextState = TopSites(oldState, action);
      assert.deepEqual(nextState, oldState);
    });
    it("should bookmark an item on PLACES_BOOKMARK_ADDED", () => {
      const oldState = { rows: [{ url: "foo.com" }, { url: "bar.com" }] };
      const action = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "bar.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 1234567,
        },
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
      const nextState = TopSites(undefined, { type: at.PLACES_BOOKMARK_ADDED });
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should remove a bookmark on PLACES_BOOKMARKS_REMOVED", () => {
      const oldState = {
        rows: [
          { url: "foo.com" },
          {
            url: "bar.com",
            bookmarkGuid: "bookmark123",
            bookmarkTitle: "Title for bar.com",
            dateAdded: 123456,
          },
        ],
      };
      const action = {
        type: at.PLACES_BOOKMARKS_REMOVED,
        data: { urls: ["bar.com"] },
      };
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
    it("should not update state for empty action.data on PLACES_BOOKMARKS_REMOVED", () => {
      const nextState = TopSites(undefined, {
        type: at.PLACES_BOOKMARKS_REMOVED,
      });
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should update prefs on TOP_SITES_PREFS_UPDATED", () => {
      const state = TopSites(
        {},
        { type: at.TOP_SITES_PREFS_UPDATED, data: { pref: "foo" } }
      );

      assert.equal(state.pref, "foo");
    });
    it("should not update state for empty action.data on PLACES_LINKS_DELETED", () => {
      const nextState = TopSites(undefined, { type: at.PLACES_LINKS_DELETED });
      assert.equal(nextState, INITIAL_STATE.TopSites);
    });
    it("should remove the site on PLACES_LINKS_DELETED", () => {
      const oldState = { rows: [{ url: "foo.com" }, { url: "bar.com" }] };
      const deleteAction = {
        type: at.PLACES_LINKS_DELETED,
        data: { urls: ["foo.com"] },
      };
      const nextState = TopSites(oldState, deleteAction);
      assert.deepEqual(nextState.rows, [{ url: "bar.com" }]);
    });
    it("should set showSearchShortcutsForm to true on TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL", () => {
      const data = { index: 7 };
      const nextState = TopSites(undefined, {
        type: at.TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL,
        data,
      });
      assert.isTrue(nextState.showSearchShortcutsForm);
    });
    it("should set showSearchShortcutsForm to false on TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL", () => {
      const nextState = TopSites(undefined, {
        type: at.TOP_SITES_CLOSE_SEARCH_SHORTCUTS_MODAL,
      });
      assert.isFalse(nextState.showSearchShortcutsForm);
    });
    it("should update searchShortcuts on UPDATE_SEARCH_SHORTCUTS", () => {
      const shortcuts = [
        {
          keyword: "@google",
          shortURL: "google",
          url: "https://google.com",
          searchIdentifier: /^google/,
        },
        {
          keyword: "@baidu",
          shortURL: "baidu",
          url: "https://baidu.com",
          searchIdentifier: /^baidu/,
        },
      ];
      const nextState = TopSites(undefined, {
        type: at.UPDATE_SEARCH_SHORTCUTS,
        data: { searchShortcuts: shortcuts },
      });
      assert.deepEqual(shortcuts, nextState.searchShortcuts);
    });
    it("should set sov positions and state", () => {
      const positions = [
        { position: 0, assignedPartner: "amp" },
        { position: 1, assignedPartner: "moz-sales" },
      ];
      const nextState = TopSites(undefined, {
        type: at.SOV_UPDATED,
        data: { ready: true, positions },
      });
      assert.equal(nextState.sov.ready, true);
      assert.equal(nextState.sov.positions, positions);
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
        const state = Prefs(undefined, {
          type: at.PREFS_INITIAL_VALUES,
          data: {},
        });
        assert.notEqual(
          INITIAL_STATE.Prefs,
          state,
          "should not modify INITIAL_STATE"
        );
      });
      it("should set initalized to true", () => {
        const state = Prefs(undefined, {
          type: at.PREFS_INITIAL_VALUES,
          data: {},
        });
        assert.isTrue(state.initialized);
      });
      it("should set .values", () => {
        const newValues = { foo: 1, bar: 2 };
        const state = Prefs(undefined, {
          type: at.PREFS_INITIAL_VALUES,
          data: newValues,
        });
        assert.equal(state.values, newValues);
      });
    });
    describe("PREF_CHANGED", () => {
      it("should return a new Prefs object", () => {
        const state = Prefs(undefined, {
          type: at.PREF_CHANGED,
          data: { name: "foo", value: 2 },
        });
        assert.notEqual(
          INITIAL_STATE.Prefs,
          state,
          "should not modify INITIAL_STATE"
        );
      });
      it("should set the changed pref", () => {
        const state = Prefs(prevState({ foo: 1 }), {
          type: at.PREF_CHANGED,
          data: { name: "foo", value: 2 },
        });
        assert.equal(state.values.foo, 2);
      });
      it("should return a new .pref object instead of mutating", () => {
        const oldState = prevState({ foo: 1 });
        const state = Prefs(oldState, {
          type: at.PREF_CHANGED,
          data: { name: "foo", value: 2 },
        });
        assert.notEqual(oldState.values, state.values);
      });
    });
  });
  describe("Dialog", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        INITIAL_STATE.Dialog,
        Dialog(undefined, { type: "non_existent" })
      );
    });
    it("should toggle visible to true on DIALOG_OPEN", () => {
      const action = { type: at.DIALOG_OPEN };
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isTrue(nextState.visible);
    });
    it("should pass url data on DIALOG_OPEN", () => {
      const action = { type: at.DIALOG_OPEN, data: "some url" };
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.equal(nextState.data, action.data);
    });
    it("should toggle visible to false on DIALOG_CANCEL", () => {
      const action = { type: at.DIALOG_CANCEL, data: "some url" };
      const nextState = Dialog(INITIAL_STATE.Dialog, action);
      assert.isFalse(nextState.visible);
    });
    it("should return inital state on DELETE_HISTORY_URL", () => {
      const action = { type: at.DELETE_HISTORY_URL };
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
        rows: [
          { url: "www.foo.bar", pocket_id: 123 },
          { url: "www.other.url" },
        ],
        order: i,
        type: "history",
      }));
    });

    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        INITIAL_STATE.Sections,
        Sections(undefined, { type: "non_existent" })
      );
    });
    it("should remove the correct section on SECTION_DEREGISTER", () => {
      const newState = Sections(oldState, {
        type: at.SECTION_DEREGISTER,
        data: "foo_bar_2",
      });
      assert.lengthOf(newState, 4);
      const expectedNewState = oldState.splice(2, 1) && oldState;
      assert.deepEqual(newState, expectedNewState);
    });
    it("should add a section on SECTION_REGISTER if it doesn't already exist", () => {
      const action = {
        type: at.SECTION_REGISTER,
        data: { id: "foo_bar_5", title: "Foo Bar 5" },
      };
      const newState = Sections(oldState, action);
      assert.lengthOf(newState, 6);
      const insertedSection = newState.find(
        section => section.id === "foo_bar_5"
      );
      assert.propertyVal(insertedSection, "title", action.data.title);
    });
    it("should set newSection.rows === [] if no rows are provided on SECTION_REGISTER", () => {
      const action = {
        type: at.SECTION_REGISTER,
        data: { id: "foo_bar_5", title: "Foo Bar 5" },
      };
      const newState = Sections(oldState, action);
      const insertedSection = newState.find(
        section => section.id === "foo_bar_5"
      );
      assert.deepEqual(insertedSection.rows, []);
    });
    it("should update a section on SECTION_REGISTER if it already exists", () => {
      const NEW_TITLE = "New Title";
      const action = {
        type: at.SECTION_REGISTER,
        data: { id: "foo_bar_2", title: NEW_TITLE },
      };
      const newState = Sections(oldState, action);
      assert.lengthOf(newState, 5);
      const updatedSection = newState.find(
        section => section.id === "foo_bar_2"
      );
      assert.ok(updatedSection && updatedSection.title === NEW_TITLE);
    });
    it("should set initialized to false on SECTION_REGISTER if there are no rows", () => {
      const NEW_TITLE = "New Title";
      const action = {
        type: at.SECTION_REGISTER,
        data: { id: "bloop", title: NEW_TITLE },
      };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "bloop");
      assert.propertyVal(updatedSection, "initialized", false);
    });
    it("should set initialized to true on SECTION_REGISTER if there are rows", () => {
      const NEW_TITLE = "New Title";
      const action = {
        type: at.SECTION_REGISTER,
        data: { id: "bloop", title: NEW_TITLE, rows: [{}, {}] },
      };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(section => section.id === "bloop");
      assert.propertyVal(updatedSection, "initialized", true);
    });
    it("should have no effect on SECTION_UPDATE if the id doesn't exist", () => {
      const action = {
        type: at.SECTION_UPDATE,
        data: { id: "fake_id", data: "fake_data" },
      };
      const newState = Sections(oldState, action);
      assert.deepEqual(oldState, newState);
    });
    it("should update the section with the correct data on SECTION_UPDATE", () => {
      const FAKE_DATA = { rows: ["some", "fake", "data"], foo: "bar" };
      const action = {
        type: at.SECTION_UPDATE,
        data: Object.assign(FAKE_DATA, { id: "foo_bar_2" }),
      };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(
        section => section.id === "foo_bar_2"
      );
      assert.include(updatedSection, FAKE_DATA);
    });
    it("should set initialized to true on SECTION_UPDATE if rows is defined on action.data", () => {
      const data = { rows: [], id: "foo_bar_2" };
      const action = { type: at.SECTION_UPDATE, data };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(
        section => section.id === "foo_bar_2"
      );
      assert.propertyVal(updatedSection, "initialized", true);
    });
    it("should retain pinned cards on SECTION_UPDATE", () => {
      const ROW = { id: "row" };
      let newState = Sections(oldState, {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows: [ROW] }, { id: "foo_bar_2" }),
      });
      let updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [ROW]);

      const PINNED_ROW = { id: "pinned", pinned: true, guid: "pinned" };
      newState = Sections(newState, {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows: [PINNED_ROW] }, { id: "foo_bar_2" }),
      });
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [PINNED_ROW]);

      // Updating the section again should not duplicate pinned cards
      newState = Sections(newState, {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows: [PINNED_ROW] }, { id: "foo_bar_2" }),
      });
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [PINNED_ROW]);

      // Updating the section should retain pinned card at its index
      newState = Sections(newState, {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows: [ROW] }, { id: "foo_bar_2" }),
      });
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, [PINNED_ROW, ROW]);

      // Clearing/Resetting the section should clear pinned cards
      newState = Sections(newState, {
        type: at.SECTION_UPDATE,
        data: Object.assign({ rows: [] }, { id: "foo_bar_2" }),
      });
      updatedSection = newState.find(section => section.id === "foo_bar_2");
      assert.deepEqual(updatedSection.rows, []);
    });
    it("should have no effect on SECTION_UPDATE_CARD if the id or url doesn't exist", () => {
      const noIdAction = {
        type: at.SECTION_UPDATE_CARD,
        data: {
          id: "non-existent",
          url: "www.foo.bar",
          options: { title: "New title" },
        },
      };
      const noIdState = Sections(oldState, noIdAction);
      const noUrlAction = {
        type: at.SECTION_UPDATE_CARD,
        data: {
          id: "foo_bar_2",
          url: "www.non-existent.url",
          options: { title: "New title" },
        },
      };
      const noUrlState = Sections(oldState, noUrlAction);
      assert.deepEqual(noIdState, oldState);
      assert.deepEqual(noUrlState, oldState);
    });
    it("should update the card with the correct data on SECTION_UPDATE_CARD", () => {
      const action = {
        type: at.SECTION_UPDATE_CARD,
        data: {
          id: "foo_bar_2",
          url: "www.other.url",
          options: { title: "Fake new title" },
        },
      };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(
        section => section.id === "foo_bar_2"
      );
      const updatedCard = updatedSection.rows.find(
        card => card.url === "www.other.url"
      );
      assert.propertyVal(updatedCard, "title", "Fake new title");
    });
    it("should only update the cards belonging to the right section on SECTION_UPDATE_CARD", () => {
      const action = {
        type: at.SECTION_UPDATE_CARD,
        data: {
          id: "foo_bar_2",
          url: "www.other.url",
          options: { title: "Fake new title" },
        },
      };
      const newState = Sections(oldState, action);
      newState.forEach((section, i) => {
        if (section.id !== "foo_bar_2") {
          assert.deepEqual(section, oldState[i]);
        }
      });
    });
    it("should allow action.data to set .initialized", () => {
      const data = { rows: [], initialized: false, id: "foo_bar_2" };
      const action = { type: at.SECTION_UPDATE, data };
      const newState = Sections(oldState, action);
      const updatedSection = newState.find(
        section => section.id === "foo_bar_2"
      );
      assert.propertyVal(updatedSection, "initialized", false);
    });
    it("should dedupe based on dedupeConfigurations", () => {
      const site = { url: "foo.com" };
      const highlights = { rows: [site], id: "highlights" };
      const topstories = { rows: [site], id: "topstories" };
      const dedupeConfigurations = [
        { id: "topstories", dedupeFrom: ["highlights"] },
      ];
      const action = { data: { dedupeConfigurations }, type: "SECTION_UPDATE" };
      const state = [highlights, topstories];

      const nextState = Sections(state, action);

      assert.equal(nextState.find(s => s.id === "highlights").rows.length, 1);
      assert.equal(nextState.find(s => s.id === "topstories").rows.length, 0);
    });
    it("should remove blocked and deleted urls from all rows in all sections", () => {
      const blockAction = {
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "www.foo.bar" },
      };
      const deleteAction = {
        type: at.PLACES_LINKS_DELETED,
        data: { urls: ["www.foo.bar"] },
      };
      const newBlockState = Sections(oldState, blockAction);
      const newDeleteState = Sections(oldState, deleteAction);
      newBlockState.concat(newDeleteState).forEach(section => {
        assert.deepEqual(section.rows, [{ url: "www.other.url" }]);
      });
    });
    it("should not update state for empty action.data on PLACES_LINK_BLOCKED", () => {
      const nextState = Sections(undefined, { type: at.PLACES_LINK_BLOCKED });
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should not update state for empty action.data on PLACES_LINKS_DELETED", () => {
      const nextState = Sections(undefined, { type: at.PLACES_LINKS_DELETED });
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should remove all removed pocket urls", () => {
      const removeAction = {
        type: at.DELETE_FROM_POCKET,
        data: { pocket_id: 123 },
      };
      const newBlockState = Sections(oldState, removeAction);
      newBlockState.forEach(section => {
        assert.deepEqual(section.rows, [{ url: "www.other.url" }]);
      });
    });
    it("should archive all archived pocket urls", () => {
      const removeAction = {
        type: at.ARCHIVE_FROM_POCKET,
        data: { pocket_id: 123 },
      };
      const newBlockState = Sections(oldState, removeAction);
      newBlockState.forEach(section => {
        assert.deepEqual(section.rows, [{ url: "www.other.url" }]);
      });
    });
    it("should not update state for empty action.data on PLACES_BOOKMARK_ADDED", () => {
      const nextState = Sections(undefined, { type: at.PLACES_BOOKMARK_ADDED });
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should bookmark an item when PLACES_BOOKMARK_ADDED is received", () => {
      const action = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "www.foo.bar",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 1234567,
        },
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
    it("should not update state for empty action.data on PLACES_BOOKMARKS_REMOVED", () => {
      const nextState = Sections(undefined, {
        type: at.PLACES_BOOKMARKS_REMOVED,
      });
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should remove the bookmark when PLACES_BOOKMARKS_REMOVED is received", () => {
      const action = {
        type: at.PLACES_BOOKMARKS_REMOVED,
        data: {
          urls: ["www.foo.bar"],
          bookmarkGuid: "bookmark123",
        },
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
      assert.equal(newRow.url, action.data.urls[0]);
      assert.equal(newRow.type, "history");
      assert.isUndefined(newRow.bookmarkGuid);
      assert.isUndefined(newRow.bookmarkTitle);
      assert.isUndefined(newRow.bookmarkDateCreated);

      // old row is unchanged
      assert.equal(oldRow, oldState[0].rows[1]);
    });
    it("should not update state for empty action.data on PLACES_SAVED_TO_POCKET", () => {
      const nextState = Sections(undefined, {
        type: at.PLACES_SAVED_TO_POCKET,
      });
      assert.equal(nextState, INITIAL_STATE.Sections);
    });
    it("should add a pocked item on PLACES_SAVED_TO_POCKET", () => {
      const action = {
        type: at.PLACES_SAVED_TO_POCKET,
        data: {
          url: "www.foo.bar",
          pocket_id: 1234,
          title: "Title for bar.com",
        },
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
      links = new Array(12).fill(null).map((v, i) => ({ url: `site${i}.com` }));
    });

    it("should place pinned links where they belong", () => {
      const pinned = [
        { url: "http://github.com/mozilla/activity-stream", title: "moz/a-s" },
        { url: "http://example.com", title: "example" },
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
        { url: "http://github.com/mozilla/activity-stream", title: "moz/a-s" },
        null,
        null,
        { url: "http://example.com", title: "example" },
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
      pinned[11] = {
        url: "http://github.com/mozilla/activity-stream",
        title: "moz/a-s",
      };
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
      const pinned = [{ url: "http://example.com" }];

      insertPinned(links, pinned);

      assert.equal(typeof pinned[0].isPinned, "undefined");
    });
  });
  describe("Pocket", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        Pocket(undefined, { type: "some_action" }),
        INITIAL_STATE.Pocket
      );
    });
    it("should set waitingForSpoc on a POCKET_WAITING_FOR_SPOC action", () => {
      const state = Pocket(undefined, {
        type: at.POCKET_WAITING_FOR_SPOC,
        data: false,
      });
      assert.isFalse(state.waitingForSpoc);
    });
    it("should have undefined for initial isUserLoggedIn state", () => {
      assert.isNull(Pocket(undefined, { type: "some_action" }).isUserLoggedIn);
    });
    it("should set isUserLoggedIn to false on a POCKET_LOGGED_IN with null", () => {
      const state = Pocket(undefined, {
        type: at.POCKET_LOGGED_IN,
        data: null,
      });
      assert.isFalse(state.isUserLoggedIn);
    });
    it("should set isUserLoggedIn to false on a POCKET_LOGGED_IN with false", () => {
      const state = Pocket(undefined, {
        type: at.POCKET_LOGGED_IN,
        data: false,
      });
      assert.isFalse(state.isUserLoggedIn);
    });
    it("should set isUserLoggedIn to true on a POCKET_LOGGED_IN with true", () => {
      const state = Pocket(undefined, {
        type: at.POCKET_LOGGED_IN,
        data: true,
      });
      assert.isTrue(state.isUserLoggedIn);
    });
    it("should set pocketCta with correct object on a POCKET_CTA", () => {
      const data = {
        cta_button: "cta button",
        cta_text: "cta text",
        cta_url: "https://cta-url.com",
        use_cta: true,
      };
      const state = Pocket(undefined, { type: at.POCKET_CTA, data });
      assert.equal(state.pocketCta.ctaButton, data.cta_button);
      assert.equal(state.pocketCta.ctaText, data.cta_text);
      assert.equal(state.pocketCta.ctaUrl, data.cta_url);
      assert.equal(state.pocketCta.useCta, data.use_cta);
    });
  });
  describe("Personalization", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        Personalization(undefined, { type: "some_action" }),
        INITIAL_STATE.Personalization
      );
    });
    it("should set lastUpdated with DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED", () => {
      const state = Personalization(undefined, {
        type: at.DISCOVERY_STREAM_PERSONALIZATION_LAST_UPDATED,
        data: {
          lastUpdated: 123,
        },
      });
      assert.equal(state.lastUpdated, 123);
    });
    it("should set initialized to true with DISCOVERY_STREAM_PERSONALIZATION_INIT", () => {
      const state = Personalization(undefined, {
        type: at.DISCOVERY_STREAM_PERSONALIZATION_INIT,
      });
      assert.equal(state.initialized, true);
    });
  });
  describe("DiscoveryStream", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        DiscoveryStream(undefined, { type: "some_action" }),
        INITIAL_STATE.DiscoveryStream
      );
    });
    it("should set isPrivacyInfoModalVisible to true with SHOW_PRIVACY_INFO", () => {
      const state = DiscoveryStream(undefined, {
        type: at.SHOW_PRIVACY_INFO,
      });
      assert.equal(state.isPrivacyInfoModalVisible, true);
    });
    it("should set isPrivacyInfoModalVisible to false with HIDE_PRIVACY_INFO", () => {
      const state = DiscoveryStream(undefined, {
        type: at.HIDE_PRIVACY_INFO,
      });
      assert.equal(state.isPrivacyInfoModalVisible, false);
    });
    it("should set layout data with DISCOVERY_STREAM_LAYOUT_UPDATE", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
        data: { layout: ["test"] },
      });
      assert.equal(state.layout[0], "test");
    });
    it("should reset layout data with DISCOVERY_STREAM_LAYOUT_RESET", () => {
      const layoutData = { layout: ["test"], lastUpdated: 123 };
      const feedsData = {
        "https://foo.com/feed1": { lastUpdated: 123, data: [1, 2, 3] },
      };
      const spocsData = {
        lastUpdated: 123,
        spocs: [1, 2, 3],
      };
      let state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_LAYOUT_UPDATE,
        data: layoutData,
      });
      state = DiscoveryStream(state, {
        type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
        data: feedsData,
      });
      state = DiscoveryStream(state, {
        type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
        data: spocsData,
      });
      state = DiscoveryStream(state, {
        type: at.DISCOVERY_STREAM_LAYOUT_RESET,
      });

      assert.deepEqual(state, INITIAL_STATE.DiscoveryStream);
    });
    it("should set config data with DISCOVERY_STREAM_CONFIG_CHANGE", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
        data: { enabled: true },
      });
      assert.deepEqual(state.config, { enabled: true });
    });
    it("should set recentSavesEnabled with DISCOVERY_STREAM_PREFS_SETUP", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_PREFS_SETUP,
        data: { recentSavesEnabled: true },
      });
      assert.isTrue(state.recentSavesEnabled);
    });
    it("should set recentSavesData with DISCOVERY_STREAM_RECENT_SAVES", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_RECENT_SAVES,
        data: { recentSaves: [1, 2, 3] },
      });
      assert.deepEqual(state.recentSavesData, [1, 2, 3]);
    });
    it("should set isUserLoggedIn with DISCOVERY_STREAM_POCKET_STATE_SET", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_POCKET_STATE_SET,
        data: { isUserLoggedIn: true },
      });
      assert.isTrue(state.isUserLoggedIn);
    });
    it("should set feeds as loaded with DISCOVERY_STREAM_FEEDS_UPDATE", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_FEEDS_UPDATE,
      });
      assert.isTrue(state.feeds.loaded);
    });
    it("should set spoc_endpoint with DISCOVERY_STREAM_SPOCS_ENDPOINT", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOCS_ENDPOINT,
        data: { url: "foo.com" },
      });
      assert.equal(state.spocs.spocs_endpoint, "foo.com");
    });
    it("should use initial state with DISCOVERY_STREAM_SPOCS_PLACEMENTS", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOCS_PLACEMENTS,
        data: {},
      });
      assert.deepEqual(state.spocs.placements, []);
    });
    it("should set placements with DISCOVERY_STREAM_SPOCS_PLACEMENTS", () => {
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOCS_PLACEMENTS,
        data: {
          placements: [1, 2, 3],
        },
      });
      assert.deepEqual(state.spocs.placements, [1, 2, 3]);
    });
    it("should set spocs with DISCOVERY_STREAM_SPOCS_UPDATE", () => {
      const data = {
        lastUpdated: 123,
        spocs: [1, 2, 3],
      };
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
        data,
      });
      assert.deepEqual(state.spocs, {
        spocs_endpoint: "",
        data: [1, 2, 3],
        lastUpdated: 123,
        loaded: true,
        frequency_caps: [],
        blocked: [],
        placements: [],
      });
    });
    it("should default to a single spoc placement", () => {
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "https://foo.com" },
      };
      const oldState = {
        spocs: {
          data: {
            spocs: {
              items: [
                {
                  url: "test-spoc.com",
                },
              ],
            },
          },
          loaded: true,
        },
        feeds: {
          data: {},
          loaded: true,
        },
      };

      const newState = DiscoveryStream(oldState, deleteAction);

      assert.equal(newState.spocs.data.spocs.items.length, 1);
    });
    it("should handle no data from DISCOVERY_STREAM_SPOCS_UPDATE", () => {
      const data = null;
      const state = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
        data,
      });
      assert.deepEqual(state.spocs, INITIAL_STATE.DiscoveryStream.spocs);
    });
    it("should add blocked spocs to blocked array with DISCOVERY_STREAM_SPOC_BLOCKED", () => {
      const firstState = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_SPOC_BLOCKED,
        data: { url: "https://foo.com" },
      });
      const secondState = DiscoveryStream(firstState, {
        type: at.DISCOVERY_STREAM_SPOC_BLOCKED,
        data: { url: "https://bar.com" },
      });
      assert.deepEqual(firstState.spocs.blocked, ["https://foo.com"]);
      assert.deepEqual(secondState.spocs.blocked, [
        "https://foo.com",
        "https://bar.com",
      ]);
    });
    it("should not update state for empty action.data on DISCOVERY_STREAM_LINK_BLOCKED", () => {
      const newState = DiscoveryStream(undefined, {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
      });
      assert.equal(newState, INITIAL_STATE.DiscoveryStream);
    });
    it("should not update state if feeds are not loaded", () => {
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "foo.com" },
      };
      const newState = DiscoveryStream(undefined, deleteAction);
      assert.equal(newState, INITIAL_STATE.DiscoveryStream);
    });
    it("should not update state if spocs and feeds data is undefined", () => {
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "foo.com" },
      };
      const oldState = {
        spocs: {
          data: {},
          loaded: true,
          placements: [{ name: "spocs" }],
        },
        feeds: {
          data: {},
          loaded: true,
        },
      };
      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(newState, oldState);
    });
    it("should remove the site on DISCOVERY_STREAM_LINK_BLOCKED from spocs if feeds data is empty", () => {
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "https://foo.com" },
      };
      const oldState = {
        spocs: {
          data: {
            spocs: {
              items: [{ url: "https://foo.com" }, { url: "test-spoc.com" }],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
        feeds: {
          data: {},
          loaded: true,
        },
      };
      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(newState.spocs.data.spocs.items, [
        { url: "test-spoc.com" },
      ]);
    });
    it("should remove the site on DISCOVERY_STREAM_LINK_BLOCKED from feeds if spocs data is empty", () => {
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "https://foo.com" },
      };
      const oldState = {
        spocs: {
          data: {},
          loaded: true,
          placements: [{ name: "spocs" }],
        },
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com" },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
      };
      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        [{ url: "test.com" }]
      );
    });
    it("should remove the site on DISCOVERY_STREAM_LINK_BLOCKED from both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com" },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [{ url: "https://foo.com" }, { url: "test-spoc.com" }],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
      };
      const deleteAction = {
        type: at.DISCOVERY_STREAM_LINK_BLOCKED,
        data: { url: "https://foo.com" },
      };
      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(newState.spocs.data.spocs.items, [
        { url: "test-spoc.com" },
      ]);
      assert.deepEqual(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        [{ url: "test.com" }]
      );
    });
    it("should not update state for empty action.data on PLACES_SAVED_TO_POCKET", () => {
      const newState = DiscoveryStream(undefined, {
        type: at.PLACES_SAVED_TO_POCKET,
      });
      assert.equal(newState, INITIAL_STATE.DiscoveryStream);
    });
    it("should add pocket_id on PLACES_SAVED_TO_POCKET in both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com" },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [{ url: "https://foo.com" }, { url: "test-spoc.com" }],
            },
          },
          placements: [{ name: "spocs" }],
          loaded: true,
        },
      };
      const action = {
        type: at.PLACES_SAVED_TO_POCKET,
        data: {
          url: "https://foo.com",
          pocket_id: 1234,
          open_url: "https://foo-1234",
        },
      };

      const newState = DiscoveryStream(oldState, action);

      assert.lengthOf(newState.spocs.data.spocs.items, 2);
      assert.equal(
        newState.spocs.data.spocs.items[0].pocket_id,
        action.data.pocket_id
      );
      assert.equal(
        newState.spocs.data.spocs.items[0].open_url,
        action.data.open_url
      );
      assert.isUndefined(newState.spocs.data.spocs.items[1].pocket_id);

      assert.lengthOf(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        2
      );
      assert.equal(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .pocket_id,
        action.data.pocket_id
      );
      assert.equal(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .open_url,
        action.data.open_url
      );
      assert.isUndefined(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[1]
          .pocket_id
      );
    });
    it("should not update state for empty action.data on DELETE_FROM_POCKET", () => {
      const newState = DiscoveryStream(undefined, {
        type: at.DELETE_FROM_POCKET,
      });
      assert.equal(newState, INITIAL_STATE.DiscoveryStream);
    });
    it("should remove site on DELETE_FROM_POCKET in both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com", pocket_id: 1234 },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [
                { url: "https://foo.com", pocket_id: 1234 },
                { url: "test-spoc.com" },
              ],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
      };
      const deleteAction = {
        type: at.DELETE_FROM_POCKET,
        data: {
          pocket_id: 1234,
        },
      };

      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(newState.spocs.data.spocs.items, [
        { url: "test-spoc.com" },
      ]);
      assert.deepEqual(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        [{ url: "test.com" }]
      );
    });
    it("should remove site on ARCHIVE_FROM_POCKET in both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com", pocket_id: 1234 },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [
                { url: "https://foo.com", pocket_id: 1234 },
                { url: "test-spoc.com" },
              ],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
      };
      const deleteAction = {
        type: at.ARCHIVE_FROM_POCKET,
        data: {
          pocket_id: 1234,
        },
      };

      const newState = DiscoveryStream(oldState, deleteAction);
      assert.deepEqual(newState.spocs.data.spocs.items, [
        { url: "test-spoc.com" },
      ]);
      assert.deepEqual(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        [{ url: "test.com" }]
      );
    });
    it("should add boookmark details on PLACES_BOOKMARK_ADDED in both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  { url: "https://foo.com" },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [{ url: "https://foo.com" }, { url: "test-spoc.com" }],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
      };
      const bookmarkAction = {
        type: at.PLACES_BOOKMARK_ADDED,
        data: {
          url: "https://foo.com",
          bookmarkGuid: "bookmark123",
          bookmarkTitle: "Title for bar.com",
          dateAdded: 1234567,
        },
      };

      const newState = DiscoveryStream(oldState, bookmarkAction);

      assert.lengthOf(newState.spocs.data.spocs.items, 2);
      assert.equal(
        newState.spocs.data.spocs.items[0].bookmarkGuid,
        bookmarkAction.data.bookmarkGuid
      );
      assert.equal(
        newState.spocs.data.spocs.items[0].bookmarkTitle,
        bookmarkAction.data.bookmarkTitle
      );
      assert.isUndefined(newState.spocs.data.spocs.items[1].bookmarkGuid);

      assert.lengthOf(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        2
      );
      assert.equal(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .bookmarkGuid,
        bookmarkAction.data.bookmarkGuid
      );
      assert.equal(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .bookmarkTitle,
        bookmarkAction.data.bookmarkTitle
      );
      assert.isUndefined(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[1]
          .bookmarkGuid
      );
    });

    it("should remove boookmark details on PLACES_BOOKMARKS_REMOVED in both feeds and spocs", () => {
      const oldState = {
        feeds: {
          data: {
            "https://foo.com/feed1": {
              data: {
                recommendations: [
                  {
                    url: "https://foo.com",
                    bookmarkGuid: "bookmark123",
                    bookmarkTitle: "Title for bar.com",
                  },
                  { url: "test.com" },
                ],
              },
            },
          },
          loaded: true,
        },
        spocs: {
          data: {
            spocs: {
              items: [
                {
                  url: "https://foo.com",
                  bookmarkGuid: "bookmark123",
                  bookmarkTitle: "Title for bar.com",
                },
                { url: "test-spoc.com" },
              ],
            },
          },
          loaded: true,
          placements: [{ name: "spocs" }],
        },
      };
      const action = {
        type: at.PLACES_BOOKMARKS_REMOVED,
        data: {
          urls: ["https://foo.com"],
        },
      };

      const newState = DiscoveryStream(oldState, action);

      assert.lengthOf(newState.spocs.data.spocs.items, 2);
      assert.isUndefined(newState.spocs.data.spocs.items[0].bookmarkGuid);
      assert.isUndefined(newState.spocs.data.spocs.items[0].bookmarkTitle);

      assert.lengthOf(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations,
        2
      );
      assert.isUndefined(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .bookmarkGuid
      );
      assert.isUndefined(
        newState.feeds.data["https://foo.com/feed1"].data.recommendations[0]
          .bookmarkTitle
      );
    });
    describe("PREF_CHANGED", () => {
      it("should set isCollectionDismissible", () => {
        const state = DiscoveryStream(undefined, {
          type: at.PREF_CHANGED,
          data: {
            name: "discoverystream.isCollectionDismissible",
            value: true,
          },
        });
        assert.equal(state.isCollectionDismissible, true);
      });
    });
  });
  describe("Search", () => {
    it("should return INITIAL_STATE by default", () => {
      assert.equal(
        Search(undefined, { type: "some_action" }),
        INITIAL_STATE.Search
      );
    });
    it("should set disable to true on DISABLE_SEARCH", () => {
      const nextState = Search(undefined, { type: "DISABLE_SEARCH" });
      assert.propertyVal(nextState, "disable", true);
    });
    it("should set focus to true on FAKE_FOCUS_SEARCH", () => {
      const nextState = Search(undefined, { type: "FAKE_FOCUS_SEARCH" });
      assert.propertyVal(nextState, "fakeFocus", true);
    });
    it("should set focus and disable to false on SHOW_SEARCH", () => {
      const nextState = Search(undefined, { type: "SHOW_SEARCH" });
      assert.propertyVal(nextState, "fakeFocus", false);
      assert.propertyVal(nextState, "disable", false);
    });
  });
  it("should set initialized to true on AS_ROUTER_INITIALIZED", () => {
    const nextState = ASRouter(undefined, { type: "AS_ROUTER_INITIALIZED" });
    assert.propertyVal(nextState, "initialized", true);
  });
});
