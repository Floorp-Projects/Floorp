// import { initSidebar } from "./browser-sidebar";
import { CustomShortcutKey } from "@nora/shared/custom-shortcut-key";
import { initStatusbar, manager } from "./common/statusbar";
import { initBrowserContextMenu } from "./common/context-menu";
import { initShareMode } from "./common/browser-share-mode";
import { initProfileManager } from "./common/profile-manager";
import { initReverseSidebarPosition } from "./common/reverse-sidebar-position";
import { initUndoClosedTab } from "./common/undo-closed-tab";
import { initPrivateContainer } from "./common/private-container";
import { setBrowserInterface } from "./common/designs/configs";
import { initDesigns } from "./common/designs";
import { initTabbar } from "./common/tabbar";

//console.log("run init");

export default function initScripts() {
  document?.addEventListener("DOMContentLoaded", () => {
    console.log("DOMContentLoaded");
  });
  console.log("load");
  initStatusbar();

  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    console.log("testButton");
    //import("./example/counter/index");
    initBrowserContextMenu();
    initTabbar();
    initDesigns();
    initShareMode();
    initProfileManager();
    initUndoClosedTab();
    initReverseSidebarPosition();

    initPrivateContainer();
    console.log("csk getinstance");
    CustomShortcutKey.getInstance();
  });
}

console.log("import index");
