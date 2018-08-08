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
          if (element && element.getRootNode({composed: true}) === context.contentWindow.document) {
            return element;
          }
          return null;
        },
      },
    };
  }
};
