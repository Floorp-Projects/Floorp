// import { initSidebar } from "./browser-sidebar";
import { CustomShortcutKey } from "@nora/shared/custom-shortcut-key";
import { initShareMode } from "./common/browser-share-mode";
import { initBrowserContextMenu } from "./common/context-menu";
import { initDesigns } from "./common/designs";
import { initDownloadbar } from "./common/downloadbar";
import { initPrivateContainer } from "./common/private-container";
import { initProfileManager } from "./common/profile-manager";
import { initReverseSidebarPosition } from "./common/reverse-sidebar-position";
import { initStatusbar } from "./common/statusbar";
import { initTabbar } from "./common/tabbar";
import { initUndoClosedTab } from "./common/undo-closed-tab";

//console.log("run init");

export default function initScripts() {
  document?.addEventListener("DOMContentLoaded", () => {
    console.log("DOMContentLoaded");
  });
  console.log("load");
  initStatusbar();
  initDownloadbar();
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
