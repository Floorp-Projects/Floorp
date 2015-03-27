/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gProfildDirFile = do_get_profile();

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource:///modules/readinglist/Sync.jsm");

let { localRecordFromServerRecord } =
  Cu.import("resource:///modules/readinglist/Sync.jsm", {});

let gList;
let gSync;
let gClient;
let gLocalItems = [];

function run_test() {
  run_next_test();
}

add_task(function* prepare() {
  gSync = Sync;
  gList = Sync.list;
  let dbFile = gProfildDirFile.clone();
  dbFile.append(gSync.list._store.pathRelativeToProfileDir);
  do_register_cleanup(function* () {
    // Wait for the list's store to close its connection to the database.
    yield gList.destroy();
    if (dbFile.exists()) {
      dbFile.remove(true);
    }
  });

  gClient = new MockClient();
  gSync._client = gClient;

  let dumpAppender = new Log.DumpAppender();
  dumpAppender.level = Log.Level.All;
  let logNames = [
    "readinglist.sync",
  ];
  for (let name of logNames) {
    let log = Log.repository.getLogger(name);
    log.level = Log.Level.All;
    log.addAppender(dumpAppender);
  }
});

add_task(function* uploadNewItems() {
  // Add some local items.
  for (let i = 0; i < 3; i++) {
    let record = {
      url: `http://example.com/${i}`,
      title: `title ${i}`,
      addedBy: "device name",
    };
    gLocalItems.push(yield gList.addItem(record));
  }

  Assert.ok(!("resolvedURL" in gLocalItems[0]._record));
  yield gSync.start();

  // The syncer should update local items with the items in the server response.
  // e.g., the item didn't have a resolvedURL before sync, but after sync it
  // should.
  Assert.ok("resolvedURL" in gLocalItems[0]._record);

  checkItems(gClient.items, gLocalItems);
});

add_task(function* uploadStatusChanges() {
  // Change an item's unread from true to false.
  Assert.ok(gLocalItems[0].unread === true);

  gLocalItems[0].unread = false;
  yield gList.updateItem(gLocalItems[0]);
  yield gSync.start();

  Assert.ok(gLocalItems[0].unread === false);
  checkItems(gClient.items, gLocalItems);
});

add_task(function* downloadChanges() {
  // Change an item on the server.
  let newTitle = "downloadChanges new title";
  let response = yield gClient.request({
    method: "PATCH",
    path: "/articles/1",
    body: {
      title: newTitle,
    },
  });
  Assert.equal(response.status, 200);

  // Add a new item on the server.
  let newRecord = {
    url: "http://example.com/downloadChanges-new-item",
    title: "downloadChanges 2",
    added_by: "device name",
  };
  response = yield gClient.request({
    method: "POST",
    path: "/articles",
    body: newRecord,
  });
  Assert.equal(response.status, 201);

  // Delete an item on the server.
  response = yield gClient.request({
    method: "DELETE",
    path: "/articles/2",
  });
  Assert.equal(response.status, 200);

  yield gSync.start();

  // Refresh the list of local items.  The changed item should be changed
  // locally, the deleted item should be deleted locally, and the new item
  // should appear in the list.
  gLocalItems = (yield gList.iterator({ sort: "guid" }).
                 items(gLocalItems.length));

  Assert.equal(gLocalItems[1].title, newTitle);
  Assert.equal(gLocalItems[2].url, newRecord.url);
  checkItems(gClient.items, gLocalItems);
});


function MockClient() {
  this._items = [];
  this._nextItemID = 0;
  this._nextLastModifiedToken = 0;
}

MockClient.prototype = {

  request(req) {
    let response = this._routeRequest(req);
    return new Promise(resolve => {
      // Resolve the promise asyncly, just as if this were a real server, so
      // that we don't somehow end up depending on sync behavior.
      setTimeout(() => {
        resolve(response);
      }, 0);
    });
  },

  get items() {
    return this._items.slice().sort((item1, item2) => {
      return item2.id < item1.id;
    });
  },

  itemByID(id) {
    return this._items.find(item => item.id == id);
  },

  itemByURL(url) {
    return this._items.find(item => item.url == url);
  },

  _items: null,
  _nextItemID: null,
  _nextLastModifiedToken: null,

  _routeRequest(req) {
    for (let prop in this) {
      let match = (new RegExp("^" + prop + "$")).exec(req.path);
      if (match) {
        let handler = this[prop];
        let method = req.method.toLowerCase();
        if (!(method in handler)) {
          throw new Error(`Handler ${prop} does not support method ${method}`);
        }
        let response = handler[method].call(this, req.body, match);
        // Make sure the response really is JSON'able (1) as a kind of sanity
        // check, (2) to convert any non-primitives (e.g., new String()) into
        // primitives, and (3) because that's what the real server returns.
        response = JSON.parse(JSON.stringify(response));
        return response;
      }
    }
    throw new Error(`Unrecognized path: ${req.path}`);
  },

  // route handlers

  "/articles": {

    get(body) {
      return new MockResponse(200, {
        // No URL params supported right now.
        items: this.items,
      });
    },

    post(body) {
      let existingItem = this.itemByURL(body.url);
      if (existingItem) {
        // The real server seems to return a 200 if the items are identical.
        if (areSameItems(existingItem, body)) {
          return new MockResponse(200);
        }
        // 303 see other
        return new MockResponse(303, {
          id: existingItem.id,
        });
      }
      body.id = new String(this._nextItemID++);
      let defaultProps = {
        last_modified: this._nextLastModifiedToken,
        preview: "",
        resolved_url: body.url,
        resolved_title: body.title,
        excerpt: "",
        archived: 0,
        deleted: 0,
        favorite: false,
        is_article: true,
        word_count: null,
        unread: true,
        added_on: null,
        stored_on: this._nextLastModifiedToken,
        marked_read_by: null,
        marked_read_on: null,
        read_position: null,
      };
      for (let prop in defaultProps) {
        if (!(prop in body) || body[prop] === null) {
          body[prop] = defaultProps[prop];
        }
      }
      this._nextLastModifiedToken++;
      this._items.push(body);
      // 201 created
      return new MockResponse(201, body);
    },
  },

  "/articles/([^/]+)": {

    get(body, routeMatch) {
      let id = routeMatch[1];
      let item = this.itemByID(id);
      if (!item) {
        return new MockResponse(404);
      }
      return new MockResponse(200, item);
    },

    patch(body, routeMatch) {
      let id = routeMatch[1];
      let item = this.itemByID(id);
      if (!item) {
        return new MockResponse(404);
      }
      for (let prop in body) {
        item[prop] = body[prop];
      }
      item.last_modified = this._nextLastModifiedToken++;
      return new MockResponse(200, item);
    },

    // There's a bug in pre-39's ES strict mode around forbidding the
    // redefinition of reserved keywords that flags defining `delete` on an
    // object as a syntax error.  This weird syntax works around that.
    ["delete"](body, routeMatch) {
      let id = routeMatch[1];
      let item = this.itemByID(id);
      if (!item) {
        return new MockResponse(404);
      }
      item.deleted = true;
      return new MockResponse(200);
    },
  },

  "/batch": {

    post(body) {
      let responses = [];
      let defaults = body.defaults || {};
      for (let request of body.requests) {
        for (let prop in defaults) {
          if (!(prop in request)) {
            request[prop] = defaults[prop];
          }
        }
        responses.push(this._routeRequest(request));
      }
      return new MockResponse(200, {
        defaults: defaults,
        responses: responses,
      });
    },
  },
};

function MockResponse(status, body, headers={}) {
  this.status = status;
  this.body = body;
  this.headers = headers;
}

function areSameItems(item1, item2) {
  for (let prop in item1) {
    if (!(prop in item2) || item1[prop] != item2[prop]) {
      return false;
    }
  }
  for (let prop in item2) {
    if (!(prop in item1) || item1[prop] != item2[prop]) {
      return false;
    }
  }
  return true;
}

function checkItems(serverRecords, localItems) {
  serverRecords = serverRecords.map(r => localRecordFromServerRecord(r));
  serverRecords = serverRecords.filter(r => !r.deleted);
  Assert.equal(serverRecords.length, localItems.length);
  for (let i = 0; i < serverRecords.length; i++) {
    for (let prop in localItems[i]._record) {
      Assert.ok(prop in serverRecords[i], prop);
      Assert.equal(serverRecords[i][prop], localItems[i]._record[prop]);
    }
  }
}
