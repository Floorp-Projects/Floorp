/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

const { TYPE_BOOKMARK, TYPE_FOLDER, TYPE_SEPARATOR } = PlacesUtils.bookmarks;

const BOOKMARKS_TYPES_TO_API_TYPES_MAP = new Map([
  [TYPE_BOOKMARK, "bookmark"],
  [TYPE_FOLDER, "folder"],
  [TYPE_SEPARATOR, "separator"],
]);

const BOOKMARK_SEPERATOR_URL = "data:";

ChromeUtils.defineLazyGetter(this, "API_TYPES_TO_BOOKMARKS_TYPES_MAP", () => {
  let theMap = new Map();

  for (let [code, name] of BOOKMARKS_TYPES_TO_API_TYPES_MAP) {
    theMap.set(name, code);
  }
  return theMap;
});

let listenerCount = 0;

function getUrl(type, url) {
  switch (type) {
    case TYPE_BOOKMARK:
      return url;
    case TYPE_SEPARATOR:
      return BOOKMARK_SEPERATOR_URL;
    default:
      return undefined;
  }
}

const getTree = (rootGuid, onlyChildren) => {
  function convert(node, parent) {
    let treenode = {
      id: node.guid,
      title: PlacesUtils.bookmarks.getLocalizedTitle(node) || "",
      index: node.index,
      dateAdded: node.dateAdded / 1000,
      type: BOOKMARKS_TYPES_TO_API_TYPES_MAP.get(node.typeCode),
      url: getUrl(node.typeCode, node.uri),
    };

    if (parent && node.guid != PlacesUtils.bookmarks.rootGuid) {
      treenode.parentId = parent.guid;
    }

    if (node.typeCode == TYPE_FOLDER) {
      treenode.dateGroupModified = node.lastModified / 1000;

      if (!onlyChildren) {
        treenode.children = node.children
          ? node.children.map(child => convert(child, node))
          : [];
      }
    }

    return treenode;
  }

  return PlacesUtils.promiseBookmarksTree(rootGuid)
    .then(root => {
      if (onlyChildren) {
        let children = root.children || [];
        return children.map(child => convert(child, root));
      }
      let treenode = convert(root, null);
      treenode.parentId = root.parentGuid;
      // It seems like the array always just contains the root node.
      return [treenode];
    })
    .catch(e => Promise.reject({ message: e.message }));
};

const convertBookmarks = result => {
  let node = {
    id: result.guid,
    title: PlacesUtils.bookmarks.getLocalizedTitle(result) || "",
    index: result.index,
    dateAdded: result.dateAdded.getTime(),
    type: BOOKMARKS_TYPES_TO_API_TYPES_MAP.get(result.type),
    url: getUrl(result.type, result.url && result.url.href),
  };

  if (result.guid != PlacesUtils.bookmarks.rootGuid) {
    node.parentId = result.parentGuid;
  }

  if (result.type == TYPE_FOLDER) {
    node.dateGroupModified = result.lastModified.getTime();
  }

  return node;
};

const throwIfRootId = id => {
  if (id == PlacesUtils.bookmarks.rootGuid) {
    throw new ExtensionError("The bookmark root cannot be modified");
  }
};

let observer = new (class extends EventEmitter {
  constructor() {
    super();
    this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
  }

  handlePlacesEvents(events) {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added":
          if (event.isTagging) {
            continue;
          }
          let bookmark = {
            id: event.guid,
            parentId: event.parentGuid,
            index: event.index,
            title: event.title,
            dateAdded: event.dateAdded,
            type: BOOKMARKS_TYPES_TO_API_TYPES_MAP.get(event.itemType),
            url: getUrl(event.itemType, event.url),
          };

          if (event.itemType == TYPE_FOLDER) {
            bookmark.dateGroupModified = bookmark.dateAdded;
          }

          this.emit("created", bookmark);
          break;
        case "bookmark-removed":
          if (event.isTagging || event.isDescendantRemoval) {
            continue;
          }
          let node = {
            id: event.guid,
            parentId: event.parentGuid,
            index: event.index,
            type: BOOKMARKS_TYPES_TO_API_TYPES_MAP.get(event.itemType),
            url: getUrl(event.itemType, event.url),
            title: event.title,
          };

          this.emit("removed", {
            guid: event.guid,
            info: { parentId: event.parentGuid, index: event.index, node },
          });
          break;
        case "bookmark-moved":
          this.emit("moved", {
            guid: event.guid,
            info: {
              parentId: event.parentGuid,
              index: event.index,
              oldParentId: event.oldParentGuid,
              oldIndex: event.oldIndex,
            },
          });
          break;
        case "bookmark-title-changed":
          if (event.isTagging) {
            continue;
          }

          this.emit("changed", {
            guid: event.guid,
            info: { title: event.title },
          });
          break;
        case "bookmark-url-changed":
          if (event.isTagging) {
            continue;
          }

          this.emit("changed", {
            guid: event.guid,
            info: { url: event.url },
          });
          break;
      }
    }
  }
})();

const decrementListeners = () => {
  listenerCount -= 1;
  if (!listenerCount) {
    PlacesUtils.observers.removeListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      observer.handlePlacesEvents
    );
  }
};

const incrementListeners = () => {
  listenerCount++;
  if (listenerCount == 1) {
    PlacesUtils.observers.addListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      observer.handlePlacesEvents
    );
  }
};

this.bookmarks = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onCreated({ fire }) {
      let listener = (event, bookmark) => {
        fire.sync(bookmark.id, bookmark);
      };

      observer.on("created", listener);
      incrementListeners();
      return {
        unregister() {
          observer.off("created", listener);
          decrementListeners();
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },

    onRemoved({ fire }) {
      let listener = (event, data) => {
        fire.sync(data.guid, data.info);
      };

      observer.on("removed", listener);
      incrementListeners();
      return {
        unregister() {
          observer.off("removed", listener);
          decrementListeners();
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },

    onChanged({ fire }) {
      let listener = (event, data) => {
        fire.sync(data.guid, data.info);
      };

      observer.on("changed", listener);
      incrementListeners();
      return {
        unregister() {
          observer.off("changed", listener);
          decrementListeners();
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },

    onMoved({ fire }) {
      let listener = (event, data) => {
        fire.sync(data.guid, data.info);
      };

      observer.on("moved", listener);
      incrementListeners();
      return {
        unregister() {
          observer.off("moved", listener);
          decrementListeners();
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    return {
      bookmarks: {
        async get(idOrIdList) {
          let list = Array.isArray(idOrIdList) ? idOrIdList : [idOrIdList];

          try {
            let bookmarks = [];
            for (let id of list) {
              let bookmark = await PlacesUtils.bookmarks.fetch({ guid: id });
              if (!bookmark) {
                throw new Error("Bookmark not found");
              }
              bookmarks.push(convertBookmarks(bookmark));
            }
            return bookmarks;
          } catch (error) {
            return Promise.reject({ message: error.message });
          }
        },

        getChildren: function (id) {
          // TODO: We should optimize this.
          return getTree(id, true);
        },

        getTree: function () {
          return getTree(PlacesUtils.bookmarks.rootGuid, false);
        },

        getSubTree: function (id) {
          return getTree(id, false);
        },

        search: function (query) {
          return PlacesUtils.bookmarks
            .search(query)
            .then(result => result.map(convertBookmarks));
        },

        getRecent: function (numberOfItems) {
          return PlacesUtils.bookmarks
            .getRecent(numberOfItems)
            .then(result => result.map(convertBookmarks));
        },

        create: function (bookmark) {
          let info = {
            title: bookmark.title || "",
          };

          info.type = API_TYPES_TO_BOOKMARKS_TYPES_MAP.get(bookmark.type);
          if (!info.type) {
            // If url is NULL or missing, it will be a folder.
            if (bookmark.url !== null) {
              info.type = TYPE_BOOKMARK;
            } else {
              info.type = TYPE_FOLDER;
            }
          }

          if (info.type === TYPE_BOOKMARK) {
            info.url = bookmark.url || "";
          }

          if (bookmark.index !== null) {
            info.index = bookmark.index;
          }

          if (bookmark.parentId !== null) {
            throwIfRootId(bookmark.parentId);
            info.parentGuid = bookmark.parentId;
          } else {
            info.parentGuid = PlacesUtils.bookmarks.unfiledGuid;
          }

          try {
            return PlacesUtils.bookmarks
              .insert(info)
              .then(convertBookmarks)
              .catch(error => Promise.reject({ message: error.message }));
          } catch (e) {
            return Promise.reject({
              message: `Invalid bookmark: ${JSON.stringify(info)}`,
            });
          }
        },

        move: function (id, destination) {
          throwIfRootId(id);
          let info = {
            guid: id,
          };

          if (destination.parentId !== null) {
            throwIfRootId(destination.parentId);
            info.parentGuid = destination.parentId;
          }
          info.index =
            destination.index === null
              ? PlacesUtils.bookmarks.DEFAULT_INDEX
              : destination.index;

          try {
            return PlacesUtils.bookmarks
              .update(info)
              .then(convertBookmarks)
              .catch(error => Promise.reject({ message: error.message }));
          } catch (e) {
            return Promise.reject({
              message: `Invalid bookmark: ${JSON.stringify(info)}`,
            });
          }
        },

        update: function (id, changes) {
          throwIfRootId(id);
          let info = {
            guid: id,
          };

          if (changes.title !== null) {
            info.title = changes.title;
          }
          if (changes.url !== null) {
            info.url = changes.url;
          }

          try {
            return PlacesUtils.bookmarks
              .update(info)
              .then(convertBookmarks)
              .catch(error => Promise.reject({ message: error.message }));
          } catch (e) {
            return Promise.reject({
              message: `Invalid bookmark: ${JSON.stringify(info)}`,
            });
          }
        },

        remove: function (id) {
          throwIfRootId(id);
          let info = {
            guid: id,
          };

          // The API doesn't give you the old bookmark at the moment
          try {
            return PlacesUtils.bookmarks
              .remove(info, { preventRemovalOfNonEmptyFolders: true })
              .catch(error => Promise.reject({ message: error.message }));
          } catch (e) {
            return Promise.reject({
              message: `Invalid bookmark: ${JSON.stringify(info)}`,
            });
          }
        },

        removeTree: function (id) {
          throwIfRootId(id);
          let info = {
            guid: id,
          };

          try {
            return PlacesUtils.bookmarks
              .remove(info)
              .catch(error => Promise.reject({ message: error.message }));
          } catch (e) {
            return Promise.reject({
              message: `Invalid bookmark: ${JSON.stringify(info)}`,
            });
          }
        },

        onCreated: new EventManager({
          context,
          module: "bookmarks",
          event: "onCreated",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "bookmarks",
          event: "onRemoved",
          extensionApi: this,
        }).api(),

        onChanged: new EventManager({
          context,
          module: "bookmarks",
          event: "onChanged",
          extensionApi: this,
        }).api(),

        onMoved: new EventManager({
          context,
          module: "bookmarks",
          event: "onMoved",
          extensionApi: this,
        }).api(),
      },
    };
  }
};
