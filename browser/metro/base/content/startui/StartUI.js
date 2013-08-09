/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var StartUI = {
  get isVisible() { return this.isStartPageVisible; },
  get isStartPageVisible() { return true; /*return Elements.windowState.hasAttribute("startpage");*/ },

  get maxResultsPerSection() {
    return Services.prefs.getIntPref("browser.display.startUI.maxresults");
  },

  sections: [
    "TopSitesStartView",
    "BookmarksStartView",
    "HistoryStartView",
    "RemoteTabsStartView"
  ],

  init: function init() {
    Elements.startUI.addEventListener("contextmenu", this, false);
    Elements.startUI.addEventListener("click", this, false);
    Elements.startUI.addEventListener("MozMousePixelScroll", this, false);

    this.sections.forEach(function (sectionName) {
      let section = window[sectionName];
      if (section.init)
        section.init();
    });

  },

  uninit: function() {
    this.sections.forEach(function (sectionName) {
      let section = window[sectionName];
      if (section.uninit)
        section.uninit();
    });
  },

  /** Show the Firefox start page / "new tab" page */
  show: function show() {
    if (this.isStartPageVisible)
      return false;

    ContextUI.displayNavbar();

    Elements.contentShowing.setAttribute("disabled", "true");
    Elements.windowState.setAttribute("startpage", "true");

    this.sections.forEach(function (sectionName) {
      let section = window[sectionName];
      if (section.show)
        section.show();
    });
    return true;
  },

  /** Hide the Firefox start page */
  hide: function hide(aURI) {
    aURI = aURI || Browser.selectedBrowser.currentURI.spec;
    if (!this.isStartPageVisible || this.isStartURI(aURI))
      return false;

    Elements.contentShowing.removeAttribute("disabled");
    Elements.windowState.removeAttribute("startpage");
    return true;
  },

  /** Is the current tab supposed to show the Firefox start page? */
  isStartURI: function isStartURI(aURI) {
    aURI = aURI || Browser.selectedBrowser.currentURI.spec;
    return aURI == kStartOverlayURI || aURI == "about:home";
  },

  /** Call this to show or hide the start page when switching tabs or pages */
  update: function update(aURI) {
    aURI = aURI || Browser.selectedBrowser.currentURI.spec;
    if (this.isStartURI(aURI)) {
      this.show();
    } else if (aURI != "about:blank") { // about:blank is loaded briefly for new tabs; ignore it
      this.hide(aURI);
    }
  },

  onClick: function onClick(aEvent) {
    // If someone clicks / taps in empty grid space, take away
    // focus from the nav bar edit so the soft keyboard will hide.
    if (BrowserUI.blurNavBar()) {
      // Advanced notice to CAO, so we can shuffle the nav bar in advance
      // of the keyboard transition.
      ContentAreaObserver.navBarWillBlur();
    }

    if (aEvent.button == 0)
      ContextUI.dismissTabs();
  },

  onNarrowTitleClick: function onNarrowTitleClick(sectionId) {
    let section = document.getElementById(sectionId);

    if (section.hasAttribute("expanded"))
      return;

    for (let expandedSection of Elements.startUI.querySelectorAll(".meta-section[expanded]"))
      expandedSection.removeAttribute("expanded")

    section.setAttribute("expanded", "true");
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        let event = document.createEvent("Events");
        event.initEvent("MozEdgeUICompleted", true, false);
        window.dispatchEvent(event);
        break;
      case "click":
        this.onClick(aEvent);
        break;

      case "MozMousePixelScroll":
        let startBox = document.getElementById("start-scrollbox");
        let [, scrollInterface] = ScrollUtils.getScrollboxFromElement(startBox);

        if (Elements.windowState.getAttribute("viewstate") == "snapped") {
          scrollInterface.scrollBy(0, aEvent.detail);
        } else {
          scrollInterface.scrollBy(aEvent.detail, 0);
        }

        aEvent.preventDefault();
        aEvent.stopPropagation();
        break;
    }
  }
};
