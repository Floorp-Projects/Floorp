"use strict";

ChromeUtils.defineModuleGetter(this, "ContextMenuChild",
                               "resource:///actors/ContextMenuChild.jsm");

this.menusChild = class extends ExtensionAPI {
  getAPI(context) {
    return {
      menus: {
        getTargetElement(targetElementId) {
          let element;
          let lastMenuTarget = ContextMenuChild.getLastTarget(context.messageManager);
          if (lastMenuTarget && Math.floor(lastMenuTarget.timeStamp) === targetElementId) {
            element = lastMenuTarget.targetRef.get();
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
