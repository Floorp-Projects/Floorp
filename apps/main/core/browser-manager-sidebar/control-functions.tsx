import { insert } from "@nora/solid-xul/solid-xul";
import type { CBrowserManagerSidebar } from ".";

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);
export class BMSControlFunctions {
  public bmsInstance: CBrowserManagerSidebar;
  constructor(BMSInstance: CBrowserManagerSidebar) {
    // Do not write DOM-related process in here.
    // This runned on create of BMSInstance.
    this.bmsInstance = BMSInstance;
  }

  visiblePanelBrowserElem() {
    const modeValuePref = this.bmsInstance.currentPanel;
    const selectedwebpanel = document.getElementById(
      `webpanel${modeValuePref}`,
    );
    const selectedURL =
      this.bmsInstance.BROWSER_SIDEBAR_DATA.panels[modeValuePref].url ?? "";
    this.changeVisibleCommandButton(selectedURL.startsWith("floorp//"));
    for (const elem of Array.from(
      document.getElementsByClassName("webpanels"),
    )) {
      elem.hidden = true;
      if (
        elem.classList.contains("isFloorp") ||
        elem.classList.contains("isExtension")
      ) {
        //TODO: WHAT?
        const src = elem.getAttribute("src");
        elem.setAttribute("src", "");
        elem.setAttribute("src", src);
      }
    }
    if (
      document.getElementById("sidebar-splitter2")?.getAttribute("hidden") ===
      "true"
    ) {
      this.changeVisibilityOfWebPanel();
    }
    this.changeCheckPanel(
      document.getElementById("sidebar-splitter2")?.getAttribute("hidden") !==
        "true",
    );
    if (selectedwebpanel != null) {
      selectedwebpanel.hidden = false;
    }
  }

  unloadWebpanel(id: string) {
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    if (id === this.bmsInstance.currentPanel) {
      this.bmsInstance.currentPanel = "";
      if (sidebarsplit2.getAttribute("hidden") !== "true") {
        this.changeVisibilityOfWebPanel();
      }
    }
    document.getElementById(`webpanel${id}`)?.remove();
    document.getElementById(`select-${id}`).removeAttribute("muted");
  }

  unloadAllWebpanel() {
    for (const elem of Array.from(
      document.getElementsByClassName("webpanels"),
    )) {
      elem.remove();
    }
    for (const elem of Array.from(
      document.getElementsByClassName("sidepanel-icon"),
    )) {
      elem.removeAttribute("muted");
    }
    this.bmsInstance.currentPanel = "";
  }

  setUserContextColorLine(id: string) {
    const webpanel_usercontext =
      this.bmsInstance.BROWSER_SIDEBAR_DATA.panels[id].usercontext ?? 0;
    const container_list = ContextualIdentityService.getPublicIdentities();
    if (
      webpanel_usercontext !== 0 &&
      container_list.findIndex(
        (e) => e.userContextId === webpanel_usercontext,
      ) !== -1
    ) {
      const container_color =
        container_list[
          container_list.findIndex(
            (e) => e.userContextId === webpanel_usercontext,
          )
        ].color;
      document.getElementById(`select-${id}`).style.borderLeft = `solid 2px ${
        container_color === "toolbar"
          ? "var(--toolbar-field-color)"
          : container_color
      }`;
    } else if (
      document.getElementById(`select-${id}`).style.border !== "1px solid blue"
    ) {
      document.getElementById(`select-${id}`).style.borderLeft = "";
    }
  }

  changeCheckPanel(doChecked: boolean) {
    for (const elem of document.getElementsByClassName("sidepanel-icon")) {
      elem.setAttribute("checked", "false");
    }
    if (doChecked) {
      const selectedNode = document.querySelector(
        `#select-${this.bmsInstance.currentPanel}`,
      );
      if (selectedNode != null) {
        selectedNode.setAttribute("checked", "true");
      }
    }
  }

  changeVisibleBrowserManagerSidebar(doVisible: boolean) {
    if (doVisible) {
      document.querySelector("html").removeAttribute("invisibleBMS");
    } else {
      document.querySelector("html").setAttribute("invisibleBMS", "true");
    }
  }

  changeVisibleCommandButton(hidden: boolean) {
    for (const elem of this.bmsInstance.sidebar_icons) {
      document.getElementById(elem).hidden = hidden;
    }
  }

  changeVisibilityOfWebPanel() {
    const siderbar2header = document.getElementById("sidebar2-header");
    const sidebarsplit2 = document.getElementById("sidebar-splitter2");
    const sidebar2box = document.getElementById("sidebar2-box");
    const sidebarSetting = {
      true: ["auto", "", "", "false"],
      false: ["0", "0", "none", "true"],
    };
    const doDisplay = sidebarsplit2.getAttribute("hidden") === "true";
    sidebar2box.style.minWidth = sidebarSetting[doDisplay][0];
    sidebar2box.style.maxWidth = sidebarSetting[doDisplay][1];
    siderbar2header.style.display = sidebarSetting[doDisplay][2];
    sidebarsplit2.setAttribute("hidden", sidebarSetting[doDisplay][3]);
    this.changeCheckPanel(doDisplay);
    Services.prefs.setBoolPref(
      "floorp.browser.sidebar.is.displayed",
      doDisplay,
    );

    // We should focus to parent window to avoid focus to webpanel
    if (!doDisplay) {
      window.focus();
    }

    if (
      Services.prefs.getBoolPref(
        "floorp.browser.sidebar2.hide.to.unload.panel.enabled",
        false,
      ) &&
      !doDisplay
    ) {
      this.unloadAllWebpanel();
    }
  }

  setSidebarWidth(webpanel_id: string) {
    if (
      webpanel_id != null &&
      Object.keys(this.bmsInstance.BROWSER_SIDEBAR_DATA.panels).includes(
        webpanel_id,
      )
    ) {
      const panelWidth =
        this.bmsInstance.BROWSER_SIDEBAR_DATA.panels[webpanel_id].width ??
        Services.prefs.getIntPref(
          "floorp.browser.sidebar2.global.webpanel.width",
          undefined,
        );
      document.getElementById("sidebar2-box").style.width = `${panelWidth}px`;
    }
  }

  visibleWebpanel() {
    const webpanel_id = this.bmsInstance.currentPanel;
    if (
      webpanel_id != null &&
      Object.keys(this.bmsInstance.BROWSER_SIDEBAR_DATA.panels).includes(
        webpanel_id,
      )
    ) {
      this.makeWebpanel(webpanel_id);
    }
  }

  makeWebpanel(webpanel_id: string) {
    const webpanelData =
      this.bmsInstance.BROWSER_SIDEBAR_DATA.panels[webpanel_id];
    const webpanobject = document.getElementById(`webpanel${webpanel_id}`);
    let webpanelURL = webpanelData.url;
    const webpanel_usercontext = webpanelData.usercontext ?? 0;
    const webpanel_userAgent = webpanelData.changeUserAgent ?? false;
    const isExtension = webpanelURL.slice(0, 9) === "extension";
    let isWeb = true;
    let isFloorp = false;
    this.setSidebarWidth(webpanel_id);

    if (webpanelURL.startsWith("floorp//")) {
      isFloorp = true;
      webpanelURL = this.bmsInstance.STATIC_SIDEBAR_DATA[webpanelURL].url;
      isWeb = false;
    }

    // // Add-on Capability
    // if (
    //   Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled") &&
    //   !isExtension
    // ) {
    //   isWeb = false;
    // }

    if (webpanobject == null) {
      // <xul:browser
      //   id="webpanel${webpanel_id}"
      //   class={`webpanels ${isFloorp ? "isFloorp" : "isWeb"} ${
      //     isExtension ? "isExtension" : ""
      //   }`}
      //   flex="1"
      //   xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
      //   disablehistory="true"
      //   disablefullscreen="true"
      //   tooltip="aHTMLTooltip"
      //   autoscroll="false"
      //   disableglobalhistory="true"
      //   messagemanagergroup="browsers"
      //   autocompletepopup="PopupAutoComplete"
      //   initialBrowsingContextGroupId="40"
      //   usercontextid={
      //     typeof webpanel_usercontext === "number"
      //       ? (String(webpanel_usercontext) as `${number}`)
      //       : "0"
      //   }
      //   changeuseragent={webpanel_userAgent ? "true" : "false"}
      //   webextension-view-type="sidebar"
      //   type="content"
      //   remote="true"
      //   maychangeremoteness="true"
      //   context=""
      // />;
      const webpanelElem = window.MozXULElement.parseXULToFragment(`
              <browser
                id="webpanel${webpanel_id}"
                class="webpanels ${isFloorp ? "isFloorp" : "isWeb"} ${
                  isExtension ? "isExtension" : ""
                }"
                flex="1"
                xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
                disablehistory="true"
                disablefullscreen="true"
                tooltip="aHTMLTooltip"
                autoscroll="false"
                disableglobalhistory="true"
                messagemanagergroup="browsers"
                autocompletepopup="PopupAutoComplete"
                initialBrowsingContextGroupId="40"
                ${
                  isWeb
                    ? `usercontextid="${
                        typeof webpanel_usercontext === "number"
                          ? String(webpanel_usercontext)
                          : "0"
                      }"
                changeuseragent="${webpanel_userAgent ? "true" : "false"}"
                webextension-view-type="sidebar"
                type="content"
                remote="true"
                maychangeremoteness="true"
                context=""
                `
                    : ""
                }
              />
                `);
      if (webpanelURL.slice(0, 9) === "extension") {
        webpanelURL = webpanelURL.split(",")[3];
      }

      // if (
      //   Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled") &&
      //   !isFloorp &&
      //   !isExtension
      // ) {
      //   webpanelElem.firstChild.setAttribute(
      //     "src",
      //     `chrome://browser/content/browser.xhtml?&floorpWebPanelId=${webpanel_id}`,
      //   );
      // } else {
      webpanelElem.firstChild.setAttribute("src", webpanelURL);
      //}
      document.getElementById("sidebar2-box").appendChild(webpanelElem);
    } else {
      if (webpanelURL.slice(0, 9) === "extension") {
        webpanelURL = webpanelURL.split(",")[3];
      }

      // if (
      //   Services.prefs.getBoolPref("floorp.browser.sidebar2.addons.enabled")
      // ) {
      //   /* empty */
      // } else {
      webpanobject.setAttribute("src", webpanelURL);
      //}
    }
    this.visiblePanelBrowserElem();
  }

  // Add Sidebar Icon to Sidebar's select box
  makeSidebarIcon() {
    for (const [i, elem] of Object.entries(
      this.bmsInstance.BROWSER_SIDEBAR_DATA.panels,
    )) {
      document.getElementById(`select-${i}`)?.remove();
      //TODO: make this feature with solid-js feature
      if (document.getElementById(`select-${i}`) == null) {
        const attr: Record<string, string> = {};
        const style: Record<string, string> = {};
        const additionalClazz = [];
        if (elem.url.startsWith("floorp//")) {
          if (elem.url in this.bmsInstance.STATIC_SIDEBAR_DATA) {
            const staticUrl =
              this.bmsInstance.STATIC_SIDEBAR_DATA[elem.url].url;
            attr["data-l10n-id"] = `show-${staticUrl}`;
            attr.context = "all-panel-context";
          } else {
            throw "unknown floorp// starting url on webpanel icon";
          }
        } else {
          additionalClazz.push("webpanel-icon");
          attr.context = "webpanel-context";
          attr.tooltiptext = elem.url;
        }
        if (elem.url.startsWith("extension")) {
          attr.tooltiptext = elem.url.split(",")[1];
          additionalClazz.push("extension-icon");
          //TODO: later
          // const listTexts =
          //   "chrome://browser/content/BMS-extension-needs-white-bg.txt";
          // const res_text = await (await fetch(listTexts)).text()

          // const lines = text.split(/\r?\n/);
          // for (const line of lines) {
          //   if (
          //     line ==
          //     gBrowserManagerSidebar.BROWSER_SIDEBAR_DATA.data[
          //       elem
          //     ].url.split(",")[2]
          //   ) {
          //     sidebarItem.className += " extension-icon-add-white";
          //     break;
          //   }
          //   }
        } else {
          style.listStyleImage = "";
        }

        const sidebarItem = (
          <xul:toolbarbutton
            id={`select-${i}`}
            class={`sidepanel-icon sicon-list ${additionalClazz.join(" ")}`}
            onClick={this.bmsInstance.selectSidebarItem}
            style={style}
            onMouseOver={this.bmsInstance.mouseEvent.mouseOver}
            onMouseOut={this.bmsInstance.mouseEvent.mouseOut}
            onDragStart={this.bmsInstance.mouseEvent.dragStart}
            onDragOver={this.bmsInstance.mouseEvent.dragOver}
            onDragLeave={this.bmsInstance.mouseEvent.dragLeave}
            onDrop={this.bmsInstance.mouseEvent.drop}
            {...attr}
          >
            <xul:image class="toolbarbutton-icon" />
            <xul:label class="toolbarbutton-text" crop="right" flex="1" />
          </xul:toolbarbutton>
        );

        insert(
          document.getElementById("panelBox"),
          () => sidebarItem,
          document.getElementById("add-button"),
        );
      } // if `select-${i}` == null
    }

    // for (const elem of document.querySelectorAll(".sidepanel-icon")) {
    //   if (elem.className.includes("webpanel-icon")) {
    //     const sbar_url =
    //       this.bmsInstance.BROWSER_SIDEBAR_DATA.data[elem.id.slice(7)].url;
    //     BrowserManagerSidebar.getFavicon(
    //       sbar_url,
    //       document.getElementById(`${elem.id}`),
    //     );
    //     this.setUserContextColorLine(elem.id.slice(7));
    //   } else {
    //     elem.style.removeProperty("--BMSIcon");
    //   }
    // }
  }

  toggleBMSShortcut() {
    //TODO: register `enable` pref
    // if (!Services.prefs.getBoolPref("floorp.browser.sidebar.enable")) {
    //   return;
    // }

    if (this.bmsInstance.currentPanel === "") {
      this.bmsInstance.currentPanel = Object.keys(
        this.bmsInstance.BROWSER_SIDEBAR_DATA.panels,
      )[0];
      this.visibleWebpanel();
      this.changeVisibilityOfWebPanel();
    }
    this.changeVisibilityOfWebPanel();
  }
}
