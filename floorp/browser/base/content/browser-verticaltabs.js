/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
 
  if (Services.prefs.getBoolPref("floorp.browser.native.verticaltabs.enabled", false)) {
    Services.prefs.setBoolPref("floorp.enable.multitab", true);

    window.setTimeout(function () {
      document.getElementsByClassName("toolbar-items")[0].id = "toolbar-items-verticaltabs";
      
      let verticalTabs = document.getElementById("toolbar-items-verticaltabs");
      let sidebarBox = document.getElementById("sidebar-box");
      //init sidebar
      sidebarBox.setAttribute("style", "overflow: scroll !important;");

      //init vertical tabs
      sidebarBox.insertBefore(verticalTabs, sidebarBox.firstChild);
      let tabBrowserArrowScrollBox = document.getElementById("tabbrowser-arrowscrollbox");
      verticalTabs.setAttribute("align", "start");
      tabBrowserArrowScrollBox.setAttribute("orient", "vertical");
      tabBrowserArrowScrollBox.removeAttribute("overflowing");
      tabBrowserArrowScrollBox.removeAttribute("scrolledtostart")
      tabBrowserArrowScrollBox.disabled = true;
      document.getElementById("tabbrowser-tabs").setAttribute("orient", "vertical");
      tabBrowserArrowScrollBox.shadowRoot.querySelector(`[part="scrollbox"]`).setAttribute("orient", "vertical");

      //Delete max-height
      let observer = new MutationObserver(function () {
        tabBrowserArrowScrollBox.shadowRoot.querySelector(`[part="scrollbox"]`).removeAttribute("style");
      })
      observer.observe(tabBrowserArrowScrollBox.shadowRoot.querySelector(`[part="scrollbox"]`), {
        attributes: true
      });
  
      //move menubar
      document.getElementById("titlebar").before(document.getElementById("toolbar-menubar"));
      if(sidebarBox.getAttribute("hidden") == "true") {
        SidebarUI.toggle();
      }

      let promise = new Promise(function (resolve, reject) {
        SidebarUI.toggle() ? resolve() : reject();
      });
      promise.then(function () {
        SidebarUI.toggle();
      });
    }, 500);

    //toolbar modification
    var Tag = document.createElement("style");
    Tag.textContent = `@import url("chrome://browser/content/browser-verticaltabs.css");`;
    document.head.appendChild(Tag);
    Services.prefs.addObserver("floorp.browser.native.verticaltabs.enabled", function () {
      if ("floorp.browser.native.verticaltabs.enabled") {
        Services.prefs.setBoolPref("floorp.enable.multitab", false);
      }
    }, false);

    /**************************************** vertical tab change position ****************************************/

    (async() => {
      let l10n = new Localization(["browser/floorp.ftl"])
      let l10n_text = await l10n.formatValue("vertical-tab-reverse-position");
      CustomizableUI.createWidget({
        id: "vertical-tab-reverse-position",
        type: "button",
        label: l10n_text,
        tooltiptext: l10n_text,
        onCreated(aNode) {
            aNode.appendChild(window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`))
    
            let a = aNode.querySelector(".toolbarbutton-icon:not(stack > .toolbarbutton-icon)")
            if (a != null) a.remove()
        },
        onCommand() {
          SidebarUI.reversePosition();
        }
      });
      CustomizableUI.addWidgetToArea("vertical-tab-reverse-position", CustomizableUI.AREA_NAVBAR, 1);
    })();
  }
