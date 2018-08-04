"use strict";

this.menusChild = class extends ExtensionAPI {
  getAPI(context) {
    return {
      menus: {
        getTargetElement(targetElementId) {
          let tabChildGlobal = context.messageManager;
          let {contextMenu} = tabChildGlobal;
          let element;
          if (contextMenu) {
            let {lastMenuTarget} = contextMenu;
            if (lastMenuTarget && Math.floor(lastMenuTarget.timeStamp) === targetElementId) {
              element = lastMenuTarget.targetRef.get();
            }
          }
          // TODO: Support shadow DOM (now we return null).
          if (element && context.contentWindow.document.contains(element)) {
            return element;
          }
          return null;
        },
      },
    };
  }
};
