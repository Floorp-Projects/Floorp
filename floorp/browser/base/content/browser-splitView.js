/* eslint-disable no-undef */
/* eslint-disable no-unused-vars */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SplitView is a feature that provides show a tab on the left or right side of the window. */

let gSplitView = {
  Functions: {
    init() {
      gSplitView.Functions.tabContextMenu.addContextMenuToTabContext();
    },
    setSplitView(tab, side) {
      let panel = gSplitView.Functions.getlinkedPanel(tab.linkedPanel);
      let browser = tab.linkedBrowser;
      let browserRenderLayers = browser.renderLayers;

      // Check if the a tab is already in split view
      let tabs = gBrowser.tabs;
      for (let i = 0; i < tabs.length; i++) {
        if (tabs[i].hasAttribute("splitView")) {
          gSplitView.Functions.removeSplitView(tabs[i]);
        }
      }

      let CSSElem = document.getElementById("splitViewCSS");
      if (!CSSElem) {
        let elem = document.createElement("style");
        elem.setAttribute("id", "splitViewCSS");
        elem.textContent = `
        #tabbrowser-tabpanels > * {
          flex: 0;
        }
        
        .deck-selected {
          flex: 1 !important;
          order: 1 !important;
        }
        
        .deck-selected[splitview="right"] {
          order: 3 !important;
        }
        
        .deck-selected[splitview="left"] {
          order: 0 !important;
        }
        
        #tabbrowser-tabpanels {
          display: flex !important;
        }
        `;
        document.head.appendChild(elem);
      }

      tab.setAttribute("splitView", true);
      panel.setAttribute("splitview", side);
      panel.setAttribute("splitviewtab", true);
      panel.classList.add("deck-selected");
      browserRenderLayers = true;

      function applySplitView() {
        browserRenderLayers = true;
        panel.classList.add("deck-selected");
      }

      window.setTimeout(applySplitView, 1000);
      window.setTimeout(applySplitView, 2000);
      window.setTimeout(applySplitView, 3000);
      window.setTimeout(applySplitView, 4000);
      window.setTimeout(applySplitView, 5000);

      gSplitView.Functions.setRenderLayersEvent();
    },

    removeSplitView(tab) {
      let panel = gSplitView.Functions.getlinkedPanel(tab.linkedPanel);
      let browser = tab.linkedBrowser;
      let browserRenderLayers = browser.renderLayers;

      // remove style
      let CSSElem = document.getElementById("splitViewCSS");
      CSSElem?.remove();

      tab.removeAttribute("splitView");
      panel.removeAttribute("splitview");
      panel.removeAttribute("splitviewtab");
      panel.classList.remove("deck-selected");
      browserRenderLayers = false;

      gSplitView.Functions.removeRenderLayersEvent();
    },

    getlinkedPanel(id) {
      let panel = document.getElementById(id);
      return panel;
    },

    setRenderLayersEvent() {
      gBrowser.tabContainer.addEventListener(
        "TabOpen",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.addEventListener(
        "TabClose",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.addEventListener(
        "TabMove",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.addEventListener(
        "TabSelect",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.addEventListener(
        "TabAttrModified",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "TabHide",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "TabShow",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "TabPinned",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "TabUnpinned",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "transitionend",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "dblclick",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "click",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "click",
        gSplitView.Functions.handleTabEvent,
        true
      );

      gBrowser.tabContainer.addEventListener(
        "keydown",
        gSplitView.Functions.handleTabEvent,
        { mozSystemGroup: true }
      );

      gBrowser.tabContainer.addEventListener(
        "dragstart",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "dragover",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "drop",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "dragend",
        gSplitView.Functions.handleTabEvent
      );

      gBrowser.tabContainer.addEventListener(
        "dragleave",
        gSplitView.Functions.handleTabEvent
      );
    },

    removeRenderLayersEvent() {
      gBrowser.tabContainer.removeEventListener(
        "TabOpen",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabClose",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabMove",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabSelect",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabAttrModified",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabHide",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabShow",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabPinned",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "TabUnpinned",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "transitionend",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "dblclick",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "click",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "click",
        gSplitView.Functions.handleTabEvent,
        true
      );
      gBrowser.tabContainer.removeEventListener(
        "keydown",
        gSplitView.Functions.handleTabEvent,
        { mozSystemGroup: true }
      );
      gBrowser.tabContainer.removeEventListener(
        "dragstart",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "dragover",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "drop",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "dragend",
        gSplitView.Functions.handleTabEvent
      );
      gBrowser.tabContainer.removeEventListener(
        "dragleave",
        gSplitView.Functions.handleTabEvent
      );
    },

    handleTabEvent() {
      let currentSplitViewTab = document.querySelector(
        `.tabbrowser-tab[splitView="true"]`
      );
      let currentSplitViewPanel = gSplitView.Functions.getlinkedPanel(
        currentSplitViewTab?.linkedPanel
      );
      let currentSplitViewBrowser = currentSplitViewTab?.linkedBrowser;

      // set renderLayers to true & Set class to deck-selected
      currentSplitViewBrowser.renderLayers = true;
      currentSplitViewPanel?.classList.add("deck-selected");

      function applySplitView() {
        currentSplitViewBrowser.renderLayers = true;
        currentSplitViewPanel?.classList.add("deck-selected");
      }

      (function modifyDeckSelectedClass() {
        let tabs = gBrowser.tabs;
        for (let i = 0; i < tabs.length; i++) {
          let panel = gSplitView.Functions.getlinkedPanel(tabs[i].linkedPanel);
          if (
            tabs[i].hasAttribute("splitView") ||
            tabs[i] == gBrowser.selectedTab
          ) {
            panel?.classList.add("deck-selected");
          } else {
            panel?.classList.remove("deck-selected");
          }
        }
      })();

      window.setTimeout(applySplitView, 1000);
    },

    tabContextMenu: {
      addContextMenuToTabContext() {
        let beforeElem = document.getElementById("context_selectAllTabs");
        let menuitemElem = window.MozXULElement.parseXULToFragment(`
               <menu id="context_splitView" data-l10n-id="floorp-split-view-menu" accesskey="D">
                   <menupopup id="splitViewTabContextMenu"
                              onpopupshowing="gSplitView.Functions.tabContextMenu.onPopupShowing(event);"/>
               </menu>
               `);
        beforeElem.before(menuitemElem);
      },

      onPopupShowing(event) {
        //delete already exsist items
        let menuElem = document.getElementById("splitViewTabContextMenu");
        while (menuElem.firstChild) {
          menuElem.firstChild.remove();
        }

        //Rebuild context menu
        if (event.target == gBrowser.selectedTab) {
          let menuItem = window.MozXULElement.parseXULToFragment(`
                   <menuitem data-l10n-id="workspace-context-menu-selected-tab" disabled="true"/>
                  `);
          let parentElem = document.getElementById("workspaceTabContextMenu");
          parentElem.appendChild(menuItem);
          return;
        }

        let menuItem = window.MozXULElement.parseXULToFragment(`
                  <menuitem id="splitViewTabContextMenuLeft" label="Left" oncommand="gSplitView.Functions.setSplitView(TabContextMenu.contextTab, 'left');"/>
                  <menuitem id="splitViewTabContextMenuRight" label="Right" oncommand="gSplitView.Functions.setSplitView(TabContextMenu.contextTab, 'right');"/>
                `);

        let parentElem = document.getElementById("splitViewTabContextMenu");
        parentElem.appendChild(menuItem);
      },

      setSplitView(event, side) {
        let tab = event.target;
        gSplitView.Functions.setSplitView(tab, side);
      },
    },
  },
};

if (Services.prefs.getBoolPref("floorp.browser.splitView.enabled")) {
  gSplitView.Functions.init();
}
