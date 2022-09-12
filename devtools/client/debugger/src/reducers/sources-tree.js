/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources tree reducer
 *
 * A Source Tree is composed of:
 *
 *  - Thread Items To designate targets/threads.
 *    These are the roots of the Tree if no project directory is selected.
 *
 *  - Group Items To designates the different domains used in the website.
 *    These are direct children of threads and may contain directory or source items.
 *
 *  - Directory Items To designate all the folders.
 *    Note that each every folder has an items. The Source Tree React component is doing the magic to coallesce folders made of only one sub folder.
 *
 *  - Source Items To designate sources.
 *    They are the leaves of the Tree. (we should not have empty directories.)
 */

const IGNORED_URLS = ["debugger eval code", "XStringBundle"];
const IGNORED_EXTENSIONS = ["css", "svg", "png"];
import { isPretty } from "../utils/source";
import { prefs } from "../utils/prefs";

export function initialSourcesTreeState() {
  return {
    // List of all Thread Tree Items.
    // All other item types are children of these and aren't store in
    // the reducer as top level objects.
    threadItems: [],

    // List of `uniquePath` of Tree Items that are expanded.
    // This should be all but Source Tree Items.
    expanded: new Set(),

    // `uniquePath` of the currently focused Tree Item.
    // It can be any type of Tree Item.
    focusedItem: null,

    // Project root set from the Source Tree.
    // This focuses the source tree on a subset of sources.
    // This is a `uniquePath`, where ${thread} is replaced by "top-level"
    // when we picked an item from the main thread. This allows to preserve
    // the root selection on page reload.
    projectDirectoryRoot: prefs.projectDirectoryRoot,
    // The name is displayed in Source Tree header
    projectDirectoryRootName: prefs.projectDirectoryRootName,

    // Reports if the top level target is a web extension.
    // If so, we should display all web extension sources.
    isWebExtension: false,

    /**
     * Boolean, to be set to true in order to display WebExtension's content scripts
     * that are applied to the current page we are debugging.
     *
     * Covered by: browser_dbg-content-script-sources.js
     * Bound to: devtools.chrome.enabled
     *
     */
    chromeAndExtensionsEnabled: prefs.chromeAndExtensionsEnabled,
  };
}

export default function update(state = initialSourcesTreeState(), action) {
  switch (action.type) {
    case "ADD_SOURCES": {
      // With this action, we only process the original sources.
      // That's because original sources don't have any direct source actor.
      const newSources = action.sources.filter(
        source =>
          source.isOriginal &&
          isSourceVisibleInSourceTree(
            source,
            state.chromeAndExtensionsEnabled,
            state.isWebExtension
          )
      );
      if (!newSources.length) {
        return state;
      }
      let changed = false;
      // Fork the array only once for all the sources
      const threadItems = [...state.threadItems];
      for (const source of newSources) {
        changed |= addSource(threadItems, source.thread, source);
      }
      if (changed) {
        return {
          ...state,
          threadItems,
        };
      }
      return state;
    }
    case "INSERT_SOURCE_ACTORS": {
      // With this action, we only cover generated sources.
      // (i.e. we need something else for sourcemapped/original sources)
      // But we do want to process source actors in order to be able to display
      // distinct Source Tree Items for sources with the same URL loaded in distinct thread.
      // (And may be also later be able to highlight the many sources with the same URL loaded in a given thread)
      const newSourceActors = action.items.filter(sourceActor =>
        isSourceVisibleInSourceTree(
          sourceActor.sourceObject,
          state.chromeAndExtensionsEnabled,
          state.isWebExtension
        )
      );
      if (!newSourceActors.length) {
        return state;
      }
      let changed = false;
      // Fork the array only once for all the sources
      const threadItems = [...state.threadItems];
      for (const sourceActor of newSourceActors) {
        // We mostly wanted to read the thread of the SourceActor,
        // most of the interesting attributes are on the Source Object.
        changed |= addSource(
          threadItems,
          sourceActor.thread,
          sourceActor.sourceObject
        );
      }
      if (changed) {
        return {
          ...state,
          threadItems,
        };
      }
      return state;
    }

    case "NAVIGATE":
      state = initialSourcesTreeState();
      // WebExtension is being special. A navigation is notified when reloading the add-on,
      // but the top level target is not being destroyed/re-created. It stays up.
      // So here, we ensure re-registering the thread so that its related sources keep
      // being displayed.
      if (action.mainThread.isWebExtension) {
        addThread(state, action.mainThread);
      }
      return state;
    case "INSERT_THREAD":
      state = { ...state };
      addThread(state, action.newThread);
      return state;
    case "REMOVE_THREAD": {
      const index = state.threadItems.findIndex(item => {
        return item.threadActorID == action.threadActorID;
      });
      if (index == -1) {
        return state;
      }
      const threadItems = [...state.threadItems];
      threadItems.splice(index, 1);
      return {
        ...state,
        threadItems,
      };
    }

    case "SET_EXPANDED_STATE":
      return updateExpanded(state, action);

    case "SET_FOCUSED_SOURCE_ITEM":
      return { ...state, focusedItem: action.item };

    case "SET_PROJECT_DIRECTORY_ROOT":
      const { url, name } = action;
      return updateProjectDirectoryRoot(state, url, name);
  }

  return state;
}

function addThread(state, thread) {
  const threadActorID = thread.actor;
  // When processing the top level target,
  // see if we are debugging an extension.
  if (thread.isTopLevel) {
    state.isWebExtension = thread.isWebExtension;
  }
  let threadItem = state.threadItems.find(item => {
    return item.threadActorID == threadActorID;
  });
  if (!threadItem) {
    threadItem = createThreadTreeItem(threadActorID);
    state.threadItems = [...state.threadItems, threadItem];
  } else {
    // We force updating the list to trigger mapStateToProps
    // as the getSourcesTreeSources selector is awaiting for the `thread` attribute
    // which we will set here.
    state.threadItems = [...state.threadItems];
  }
  // Inject the reducer thread object on Thread Tree Items
  // (this is handy shortcut to have access to from React components)
  threadItem.thread = thread;
}

function updateExpanded(state, action) {
  // We receive the full list of all expanded items
  // (not only the one added/removed)
  return {
    ...state,
    expanded: new Set(action.expanded),
  };
}

/**
 * Update the project directory root
 */
function updateProjectDirectoryRoot(state, root, name) {
  // Only persists root within the top level target.
  // Otherwise the thread actor ID will change on page reload and we won't match anything
  if (!root || root.startsWith("top-level")) {
    prefs.projectDirectoryRoot = root;
    prefs.projectDirectoryRootName = name;
  }

  return {
    ...state,
    projectDirectoryRoot: root,
    projectDirectoryRootName: name,
  };
}

function isSourceVisibleInSourceTree(
  source,
  chromeAndExtensionsEnabled,
  debuggeeIsWebExtension
) {
  return (
    !!source.url &&
    !IGNORED_EXTENSIONS.includes(source.displayURL.fileExtension) &&
    !IGNORED_URLS.includes(source.url) &&
    !isPretty(source) &&
    // Only accept web extension sources when the chrome pref is enabled (to allows showing content scripts),
    // or when we are debugging an extension
    (!source.isExtension ||
      chromeAndExtensionsEnabled ||
      debuggeeIsWebExtension)
  );
}

function addSource(threadItems, thread, source) {
  // Ensure creating or fetching the related Thread Item
  let threadItem = threadItems.find(item => {
    return item.threadActorID == thread;
  });
  if (!threadItem) {
    threadItem = createThreadTreeItem(thread);
    // Note that threadItems will be cloned once to force a state update
    // by the callsite of `addSourceActor`
    threadItems.push(threadItem);
  }

  // Then ensure creating or fetching the related Group Item
  // About `source` versus `sourceActor`:
  const { displayURL } = source;
  const { group } = displayURL;

  let groupItem = threadItem.children.find(item => {
    return item.groupName == group;
  });
  if (!groupItem) {
    groupItem = createGroupTreeItem(group, thread, threadItem, source);
    // Copy children in order to force updating react in case we picked
    // this directory as a project root
    threadItem.children = [...threadItem.children, groupItem];
    // As we add a new item, re-sort the groups in this thread
    threadItem.children.sort(sortItems);
  }

  // Then ensure creating or fetching all possibly nested Directory Item(s)
  const { path } = displayURL;
  const parentPath = path.substring(0, path.lastIndexOf("/"));
  const directoryItem = addOrGetParentDirectory(thread, groupItem, parentPath);

  // Check if a previous source actor registered this source.
  // It happens if we load the same url multiple times, or,
  // for inline sources (=HTML pages with inline scripts).
  const existing = directoryItem.children.find(item => {
    return item.type == "source" && item.source == source;
  });
  if (existing) {
    return false;
  }

  // Finaly, create the Source Item and register it in its parent Directory Item
  const sourceItem = createSourceTreeItem(source, thread, group, directoryItem);
  // Copy children in order to force updating react in case we picked
  // this directory as a project root
  directoryItem.children = [...directoryItem.children, sourceItem];
  // Re-sort the items in this directory
  directoryItem.children.sort(sortItems);

  return true;
}

function sortItems(a, b) {
  if (a.type == "directory" && b.type == "source") {
    return -1;
  } else if (b.type == "directory" && a.type == "source") {
    return 1;
  } else if (a.type == "directory" && b.type == "directory") {
    return a.path.localeCompare(b.path);
  } else if (a.type == "source" && b.type == "source") {
    return a.source.displayURL.filename.localeCompare(
      b.source.displayURL.filename
    );
  }
  return 0;
}

/**
 * For a given URL's path, in the given group (i.e. typically a given scheme+domain),
 * return the already existing parent directory item, or create it if it doesn't exists.
 * Note that it will create all ancestors up to the Group Item.
 *
 * @param {GroupItem} groupItem
 *        The Group Item for the group where the path should be displayed.
 * @param {String} path
 *        Path of the directory for which we want a Directory Item.
 * @return {GroupItem|DirectoryItem}
 *        The parent Item where this path should be inserted.
 *        Note that it may be displayed right under the Group Item if the path is empty.
 */
function addOrGetParentDirectory(thread, groupItem, path) {
  // We reached the top of the Tree, so return the Group Item.
  if (!path) {
    return groupItem;
  }
  // See if we have this directory already registered by a previous source
  const existing = groupItem._allGroupDirectoryItems.find(item => {
    return item.type == "directory" && item.path == path;
  });
  if (existing) {
    return existing;
  }
  // It doesn't exists, so we will create a new Directory Item.
  // But now, lookup recursively for the parent Item for this to-be-create Directory Item
  const parentPath = path.substring(0, path.lastIndexOf("/"));
  const parentDirectory = addOrGetParentDirectory(
    thread,
    groupItem,
    parentPath
  );

  // We can now create the new Directory Item and register it in its parent Item.
  const directory = createDirectoryTreeItem(
    path,
    thread,
    groupItem.groupName,
    parentDirectory
  );
  // Copy children in order to force updating react in case we picked
  // this directory as a project root
  parentDirectory.children = [...parentDirectory.children, directory];
  // Re-sort the items in this directory
  parentDirectory.children.sort(sortItems);

  // Also maintain the list of all group items,
  // Which helps speedup querying for existing items.
  groupItem._allGroupDirectoryItems.push(directory);

  return directory;
}

/**
 * Definition of all Items of a SourceTree
 */
// Highlights the attributes that all Source Tree Item should expose
function createBaseTreeItem({ type, parent, uniquePath, children }) {
  return {
    // Can be: thread, group, directory or source
    type,
    // Reference to the parent TreeItem
    parent,
    // This attribute is used for two things:
    // * as a string key identified in the React Tree
    // * for project root in order to find the root in the tree
    // It is of the form:
    // `${ThreadActorID}|${GroupName}|${DirectoryPath}|${SourceID}`
    // Group and path/ID are optional.
    // `|` is used as separator in order to avoid having this character being used in name/path/IDs.
    uniquePath,
    // Array of TreeItem, children of this item.
    // Will be null for Source Tree Item
    children,
  };
}
function createThreadTreeItem(thread) {
  return {
    ...createBaseTreeItem({
      type: "thread",
      // Each thread is considered as an independant root item
      parent: null,
      uniquePath: thread,
      // Children of threads will only be Group Items
      children: [],
    }),

    // This will be used to set the reducer's thread object.
    // This threadActorID attribute isn't meant to be used outside of this selector.
    // A `thread` attribute will be exposed from INSERT_THREAD action.
    threadActorID: thread,
  };
}
function createGroupTreeItem(groupName, thread, parent, source) {
  return {
    ...createBaseTreeItem({
      type: "group",
      parent,
      uniquePath: `${parent.uniquePath}|${groupName}`,
      // Children of Group can be Directory and Source items
      children: [],
    }),

    groupName,

    // When a content script appear in a web page,
    // a dedicated group is created for it and should
    // be having an extension icon.
    isForExtensionSource: source.isExtension,

    // List of all nested items for this group.
    // This helps find any nested directory in a given group without having to walk the tree.
    // This is meant to be used only by the reducer.
    _allGroupDirectoryItems: [],
  };
}
function createDirectoryTreeItem(path, thread, groupName, parent) {
  // If the parent is a group we want to use '/' as separator
  const pathSeparator = parent.type == "directory" ? "/" : "|";

  // `path` will be the absolute path from the group/domain,
  // while we want to append only the directory name in uniquePath.
  // Also, we need to strip '/' prefix.
  const relativePath =
    parent.type == "directory"
      ? path.replace(parent.path, "").replace(/^\//, "")
      : path;

  return {
    ...createBaseTreeItem({
      type: "directory",
      parent,
      uniquePath: `${parent.uniquePath}${pathSeparator}${relativePath}`,
      // Children can be nested Directory or Source items
      children: [],
    }),

    // This is the absolute path from the "group"
    // i.e. the path from the domain name
    // For http://mozilla.org/foo/bar folder,
    // path will be:
    //   foo/bar
    path,
  };
}
function createSourceTreeItem(source, thread, groupName, parent) {
  return {
    ...createBaseTreeItem({
      type: "source",
      parent,
      uniquePath: `${parent.uniquePath}|${source.id}`,
      // Sources items are leaves of the SourceTree
      children: null,
    }),

    source,
  };
}
