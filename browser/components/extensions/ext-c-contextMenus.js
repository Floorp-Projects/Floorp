/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// If id is not specified for an item we use an integer.
// This ID need only be unique within a single addon. Since all addon code that
// can use this API runs in the same process, this local variable suffices.
var gNextMenuItemID = 0;

// Map[Extension -> Map[string or id, ContextMenusClickPropHandler]]
var gPropHandlers = new Map();

// The contextMenus API supports an "onclick" attribute in the create/update
// methods to register a callback. This class manages these onclick properties.
class ContextMenusClickPropHandler {
  constructor(context) {
    this.context = context;
    // Map[string or integer -> callback]
    this.onclickMap = new Map();
    this.dispatchEvent = this.dispatchEvent.bind(this);
  }

  // A listener on contextMenus.onClicked that forwards the event to the only
  // listener, if any.
  dispatchEvent(info, tab) {
    let onclick = this.onclickMap.get(info.menuItemId);
    if (onclick) {
      // No need for runSafe or anything because we are already being run inside
      // an event handler -- the event is just being forwarded to the actual
      // handler.
      onclick(info, tab);
    }
  }

  // Sets the `onclick` handler for the given menu item.
  // The `onclick` function MUST be owned by `this.context`.
  setListener(id, onclick) {
    if (this.onclickMap.size === 0) {
      this.context.childManager.getParentEvent("contextMenus.onClicked").addListener(this.dispatchEvent);
      this.context.callOnClose(this);
    }
    this.onclickMap.set(id, onclick);

    let propHandlerMap = gPropHandlers.get(this.context.extension);
    if (!propHandlerMap) {
      propHandlerMap = new Map();
    } else {
      // If the current callback was created in a different context, remove it
      // from the other context.
      let propHandler = propHandlerMap.get(id);
      if (propHandler && propHandler !== this) {
        propHandler.unsetListener(id);
      }
    }
    propHandlerMap.set(id, this);
    gPropHandlers.set(this.context.extension, propHandlerMap);
  }

  // Deletes the `onclick` handler for the given menu item.
  // The `onclick` function MUST be owned by `this.context`.
  unsetListener(id) {
    if (!this.onclickMap.delete(id)) {
      return;
    }
    if (this.onclickMap.size === 0) {
      this.context.childManager.getParentEvent("contextMenus.onClicked").removeListener(this.dispatchEvent);
      this.context.forgetOnClose(this);
    }
    let propHandlerMap = gPropHandlers.get(this.context.extension);
    propHandlerMap.delete(id);
    if (propHandlerMap.size === 0) {
      gPropHandlers.delete(this.context.extension);
    }
  }

  // Deletes the `onclick` handler for the given menu item, if any, regardless
  // of the context where it was created.
  unsetListenerFromAnyContext(id) {
    let propHandlerMap = gPropHandlers.get(this.context.extension);
    let propHandler = propHandlerMap && propHandlerMap.get(id);
    if (propHandler) {
      propHandler.unsetListener(id);
    }
  }

  // Remove all `onclick` handlers of the extension.
  deleteAllListenersFromExtension() {
    let propHandlerMap = gPropHandlers.get(this.context.extension);
    if (propHandlerMap) {
      for (let [id, propHandler] of propHandlerMap) {
        propHandler.unsetListener(id);
      }
    }
  }

  // Removes all `onclick` handlers from this context.
  close() {
    for (let id of this.onclickMap.keys()) {
      this.unsetListener(id);
    }
  }
}

this.contextMenus = class extends ExtensionAPI {
  getAPI(context) {
    let onClickedProp = new ContextMenusClickPropHandler(context);

    return {
      contextMenus: {
        create(createProperties, callback) {
          if (createProperties.id === null) {
            createProperties.id = ++gNextMenuItemID;
          }
          let {onclick} = createProperties;
          delete createProperties.onclick;
          context.childManager.callParentAsyncFunction("contextMenus.createInternal", [
            createProperties,
          ]).then(() => {
            if (onclick) {
              onClickedProp.setListener(createProperties.id, onclick);
            }
            if (callback) {
              callback();
            }
          });
          return createProperties.id;
        },

        update(id, updateProperties) {
          let {onclick} = updateProperties;
          delete updateProperties.onclick;
          return context.childManager.callParentAsyncFunction("contextMenus.update", [
            id,
            updateProperties,
          ]).then(() => {
            if (onclick) {
              onClickedProp.setListener(id, onclick);
            } else if (onclick === null) {
              onClickedProp.unsetListenerFromAnyContext(id);
            }
            // else onclick is not set so it should not be changed.
          });
        },

        remove(id) {
          onClickedProp.unsetListenerFromAnyContext(id);
          return context.childManager.callParentAsyncFunction("contextMenus.remove", [
            id,
          ]);
        },

        removeAll() {
          onClickedProp.deleteAllListenersFromExtension();

          return context.childManager.callParentAsyncFunction("contextMenus.removeAll", []);
        },
      },
    };
  }
};
