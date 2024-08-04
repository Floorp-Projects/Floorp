import { BMSContextMenu } from "./context-menu";
import { BMSControlFunctions } from "./control-functions";
import { BMSMouseEvent } from "./mouse-event";
import { BMSUtils } from "./BrowserManagerSidebar";

import { sidebar } from "./browser-manager-sidebar";
import { sidebarContext } from "./browser-manager-sidebar-context";
import { sidebarSelectBox } from "@content/browser/browser-webpanel/browser-webpanel";

import { Sidebar3Data } from "./SidebarData";
import { PanelWindowUtils } from "@private/browser-manager-sidebar/PanelWindowUtils";
import { render } from "@nora/solid-xul/solid-xul";

export class CBrowserManagerSidebar {
  private static instance: CBrowserManagerSidebar;

  controlFunctions = new BMSControlFunctions(this);
  contextMenu = new BMSContextMenu(this);
  mouseEvent = new BMSMouseEvent();
  utils = new BMSUtils();
  panelWindowUtils = new PanelWindowUtils(this);

  //TODO: this to empty string
  currentPanel = "floorp__bookmarks";
  clickedWebpanel = "";
  webpanel = "";
  contextWebpanel: Element | null = null;
  public static getInstance() {
    if (!CBrowserManagerSidebar.instance) {
      CBrowserManagerSidebar.instance = new CBrowserManagerSidebar();
    }
    return CBrowserManagerSidebar.instance;
  }

  get STATIC_SIDEBAR_DATA() {
    return this.utils.STATIC_SIDEBAR_DATA;
  }

  get BROWSER_SIDEBAR_DATA() {
    return Sidebar3Data.parse(
      JSON.parse(
        Services.prefs.getStringPref("floorp.browser.sidebar2.data", undefined),
      ),
    );
  }
  get sidebar_icons() {
    return [
      "sidebar2-back",
      "sidebar2-forward",
      "sidebar2-reload",
      "sidebar2-home",
    ];
  }
  getWebpanelIdBySelectedButtonId(selectId: string) {
    return selectId.replace("select-", "webpanel");
  }

  getSelectIdByWebpanelId(webpanelId: string) {
    return `select-${webpanelId}`;
  }

  getWebpanelObjectById(webpanelId: string) {
    return webpanelId.replace("select-", "");
  }

  private constructor() {
    inject(this);
    if (!Services.prefs.prefHasDefaultValue("floorp.browser.sidebar2.data")) {
      this.utils.updatePrefs();
    }

    Services.prefs.addObserver(
      "floorp.browser.sidebar2.global.webpanel.width",
      () => this.controlFunctions.setSidebarWidth(this.currentPanel),
    );
    Services.prefs.addObserver("floorp.browser.sidebar.enable", () =>
      this.controlFunctions.changeVisibleBrowserManagerSidebar(
        Services.prefs.getBoolPref("floorp.browser.sidebar.enable", true),
      ),
    );
    this.controlFunctions.changeVisibleBrowserManagerSidebar(
      Services.prefs.getBoolPref("floorp.browser.sidebar.enable", true),
    );
    // Services.prefs.addObserver("floorp.browser.sidebar2.data", () => {
    //   for (const elem of this.BROWSER_SIDEBAR_DATA.index) {
    //     if (
    //       document.querySelector(`#webpanel${elem}`) &&
    //       JSON.stringify(this.BROWSER_SIDEBAR_DATA.data[elem])
    //     ) {
    //       if (
    //         this.currentPanel === elem &&
    //         !(sidebarsplit2.getAttribute("hidden") === "true")
    //       ) {
    //         this.controlFunctions.makeWebpanel(elem);
    //       } else {
    //         this.controlFunctions.unloadWebpanel(elem);
    //       }
    //     }
    //   }
    //   this.controlFunctions.makeSidebarIcon();
    // });
    Services.obs.addObserver(this.servicesObs, "obs-panel-re");
    Services.obs.addObserver(
      this.controlFunctions.changeVisibilityOfWebPanel,
      "floorp-change-panel-show",
    );
    const addbutton = document.getElementById("add-button");
    // //TODO: remove when addbutton is added
    // if (addbutton) {
    //   addbutton.ondragover = this.mouseEvent.dragOver;
    //   addbutton.ondragleave = this.mouseEvent.dragLeave;
    //   addbutton.ondrop = this.mouseEvent.drop;
    // }

    //startup functions
    this.controlFunctions.makeSidebarIcon();
    // sidebar display
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    if (!(sidebarsplit2.getAttribute("hidden") === "true")) {
      this.controlFunctions.changeVisibilityOfWebPanel();
    }

    //TODO: Remove this
    this.controlFunctions.makeWebpanel("floorp__bookmarks");
  }

  // Sidebar button functions
  sidebarButtons(action: "back" | "forward" | "reload" | "home") {
    const modeValuePref = this.currentPanel;
    const webpanel = document.getElementById(`webpanel${modeValuePref}`);
    switch (action) {
      case "back":
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          this.panelWindowUtils.goBackPanel(window, webPanelId, true);
        } else {
          webpanel.goBack();
        }
        break;
      case "forward":
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          this.panelWindowUtils.goForwardPanel(window, webPanelId, true);
        } else {
          webpanel.goForward();
        }
        break;
      case "reload":
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          this.panelWindowUtils.reloadPanel(window, webPanelId, true);
        } else {
          webpanel.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
        }
        break;
      case "home":
        if (webpanel.src.startsWith("chrome://browser/content/browser.xhtml")) {
          const webPanelId = webpanel.id.replace("webpanel", "");
          this.panelWindowUtils.goIndexPagePanel(window, webPanelId, true);
        } else {
          webpanel.gotoIndex();
        }
        break;
    }
    this.controlFunctions.makeWebpanel("floorp__bookmarks");
  }

  // keep sidebar width for each webpanel
  keepWebPanelWidth() {
    const pref = this.currentPanel;
    const currentBSD = this.BROWSER_SIDEBAR_DATA;
    currentBSD.panels[pref].width =
      document.getElementById("sidebar2-box").clientWidth;
    Services.prefs.setStringPref(
      "floorp.browser.sidebar2.data",
      JSON.stringify(currentBSD),
    );
  }

  // keep sidebar width for global
  keepWidthToGlobalValue() {
    Services.prefs.setIntPref(
      "floorp.browser.sidebar2.global.webpanel.width",
      document.getElementById("sidebar2-box").width,
    );
  }

  // Services Observer
  servicesObs(_data) {
    const data = _data.wrappedJSObject;
    switch (data.eventType) {
      case "mouseOver":
        document.getElementById(
          data.id.replace("BSB-", "select-"),
        ).style.border = "1px solid blue";
        this.controlFunctions.setUserContextColorLine(
          data.id.replace("BSB-", ""),
        );
        break;
      case "mouseOut":
        document.getElementById(
          data.id.replace("BSB-", "select-"),
        ).style.border = "";
        this.controlFunctions.setUserContextColorLine(
          data.id.replace("BSB-", ""),
        );
        break;
    }
  }

  selectSidebarItem(event) {
    const custom_url_id = event.target.id.replace("select-", "");
    if (this.currentPanel === custom_url_id) {
      this.controlFunctions.changeVisibilityOfWebPanel();
    } else {
      this.currentPanel = custom_url_id;
      this.controlFunctions.visibleWebpanel();
    }
  }
}

function inject(bms: CBrowserManagerSidebar) {
  render(() => sidebar(bms), document.getElementById("@nora:appcontent"));
  render(
    () => sidebarSelectBox(bms),
    document.getElementById("@nora:appcontent"),
  );
  render(
    () => sidebarContext(bms),
    document.getElementById("@nora:window-modal-dialog"),
  );
}
// function onDOMLoad() {}

// document.addEventListener("DOMContentLoaded", () => {

//   //init
//   //@ts-ignore

// });
