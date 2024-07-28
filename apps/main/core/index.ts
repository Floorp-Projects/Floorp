// import { initSidebar } from "./browser-sidebar";
import { CustomShortcutKey } from "@nora/shared/custom-shortcut-key";
import { initStatusbar } from "./common/statusbar";
import { initBrowserContextMenu } from "./common/context-menu";
import { initShareMode } from "./common/browser-share-mode";
import { initProfileManager } from "./common/profile-manager";
import { initReverseSidebarPosition } from "./common/reverse-sidebar-position";
import { initUndoClosedTab } from "./common/undo-closed-tab";
import { initPrivateContainer } from "./common/private-container";
import { setBrowserInterface } from "./common/designs/configs";
import { initDesigns } from "./common/designs";

console.log("run init");

export default function initScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(() => {
    console.log("testButton");
    import("./example/counter/index");
    initBrowserContextMenu();
    initDesigns();

    initShareMode();
    initProfileManager();
    initUndoClosedTab();
    initReverseSidebarPosition();

    //createWebpanel("tmp", "https://manatoki332.net/");
    //console.log(document.getElementById("tmp"));
    //window.gBrowserManagerSidebar = CBrowserManagerSidebar.getInstance();

    initStatusbar();
    initPrivateContainer();
    console.log("csk getinstance");
    CustomShortcutKey.getInstance();
    //initSidebar();

    window.gFloorp = {
      design: {
        setInterface: setBrowserInterface,
      },
    };
  });
}

console.log("import index");
