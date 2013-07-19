/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Appbar = {
  get starButton()    { return document.getElementById('star-button'); },
  get pinButton()     { return document.getElementById('pin-button'); },
  get menuButton()    { return document.getElementById('menu-button'); },

  // track selected/active richgrid/tilegroup - the context for contextual action buttons
  activeTileset: null,

  init: function Appbar_init() {
    // fired from appbar bindings
    Elements.contextappbar.addEventListener('MozAppbarShowing', this, false);
    Elements.contextappbar.addEventListener('MozAppbarDismissing', this, false);

    // fired when a context sensitive item (bookmarks) changes state
    window.addEventListener('MozContextActionsChange', this, false);

    // browser events we need to update button state on
    Elements.browsers.addEventListener('URLChanged', this, true);
    Elements.tabList.addEventListener('TabSelect', this, true);

    // tilegroup selection events for all modules get bubbled up
    window.addEventListener("selectionchange", this, false);
  },

  handleEvent: function Appbar_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'URLChanged':
      case 'TabSelect':
      case 'MozAppbarShowing':
        this.update();
        break;

      case 'MozAppbarDismissing':
        if (this.activeTileset) {
          this.activeTileset.clearSelection();
        }
        this.clearContextualActions();
        this.activeTileset = null;
        break;

      case 'MozContextActionsChange':
        let actions = aEvent.actions;
        let noun = aEvent.noun;
        let qty = aEvent.qty;
        // could transition in old, new buttons?
        this.showContextualActions(actions, noun, qty);
        break;

      case "selectionchange":
        let nodeName = aEvent.target.nodeName;
        if ('richgrid' === nodeName) {
          this._onTileSelectionChanged(aEvent);
        }
        break;
    }
  },

  /*
   * Called from various places when the visible content
   * has changed such that button states may need to be
   * updated.
   */
  update: function update() {
    this._updatePinButton();
    this._updateStarButton();
  },

  onDownloadButton: function() {
    // TODO: Bug 883962: Toggle the downloads infobar when the
    // download button is clicked
    ContextUI.dismiss();
  },

  onPinButton: function() {
    if (this.pinButton.checked) {
      Browser.pinSite();
    } else {
      Browser.unpinSite();
    }
  },

  onStarButton: function(aValue) {
    if (aValue === undefined) {
      aValue = this.starButton.checked;
    }

    if (aValue) {
      Browser.starSite(function () {
        Appbar._updateStarButton();
      });
    } else {
      Browser.unstarSite(function () {
        Appbar._updateStarButton();
      });
    }
  },

  onMenuButton: function(aEvent) {
      var typesArray = ["find-in-page"];

      if (ConsolePanelView.enabled) typesArray.push("open-error-console");
      if (!MetroUtils.immersive) typesArray.push("open-jsshell");

      try {
        // If we have a valid http or https URI then show the view on desktop
        // menu item.
        var uri = Services.io.newURI(Browser.selectedBrowser.currentURI.spec,
                                     null, null);
        if (uri.schemeIs('http') || uri.schemeIs('https')) {
          typesArray.push("view-on-desktop");
        }
      } catch(ex) {
      }

      var x = this.menuButton.getBoundingClientRect().left;
      var y = Elements.toolbar.getBoundingClientRect().top;
      ContextMenuUI.showContextMenu({
        json: {
          types: typesArray,
          string: '',
          xPos: x,
          yPos: y,
          leftAligned: true,
          bottomAligned: true
      }

      });
  },

  onViewOnDesktop: function() {
    try {
      // Make sure we have a valid URI so Windows doesn't prompt
      // with an unrecognized command, select default program window
      var uri = Services.io.newURI(Browser.selectedBrowser.currentURI.spec,
                                   null, null);
      if (uri.schemeIs('http') || uri.schemeIs('https')) {
        MetroUtils.launchInDesktop(Browser.selectedBrowser.currentURI.spec, "");
      }
    } catch(ex) {
    }
  },

  dispatchContextualAction: function(aActionName){
    let activeTileset = this.activeTileset;
    if (activeTileset) {
      // fire event on the richgrid, others can listen
      // but we keep coupling loose so grid doesn't need to know about appbar
      let event = document.createEvent("Events");
      event.action = aActionName;
      event.initEvent("context-action", true, true); // is cancelable
      activeTileset.dispatchEvent(event);
      if (!event.defaultPrevented) {
        activeTileset.clearSelection();
        Elements.contextappbar.dismiss();
      }
    }
  },

  showContextualActions: function(aVerbs, aNoun, aQty) {
    // When the appbar is not visible, we want the icons to refresh right away
    let immediate = !Elements.contextappbar.isShowing;

    if (aVerbs.length)
      Elements.contextappbar.show();
    else
      Elements.contextappbar.hide();

    // Look up all of the buttons for the verbs that should be visible.
    let idsToVisibleVerbs = new Map();
    for (let verb of aVerbs) {
      let id = verb + "-selected-button";
      if (!document.getElementById(id)) {
        throw new Error("Appbar.showContextualActions: no button for " + verb);
      }
      idsToVisibleVerbs.set(id, verb);
    }

    // Sort buttons into 2 buckets - needing showing and needing hiding.
    let toHide = [], toShow = [];
    let buttons = Elements.contextappbar.getElementsByTagName("toolbarbutton");
    for (let button of buttons) {
      let verb = idsToVisibleVerbs.get(button.id);
      if (verb != undefined) {
        // Button should be visible, and may or may not be showing.
        this._updateContextualActionLabel(button, verb, aNoun, aQty);
        if (button.hidden) {
          toShow.push(button);
        }
      } else if (!button.hidden) {
        // Button is visible, but shouldn't be.
        toHide.push(button);
      }
    }

    if (immediate) {
      toShow.forEach(function(element) {
        element.removeAttribute("fade");
        element.hidden = false;
      });
      toHide.forEach(function(element) {
        element.setAttribute("fade", true);
        element.hidden = true;
      });
      return;
    }

    return Task.spawn(function() {
      if (toHide.length) {
        yield Util.transitionElementVisibility(toHide, false);
      }
      if (toShow.length) {
        yield Util.transitionElementVisibility(toShow, true);
      }
    });
  },

  clearContextualActions: function() {
    this.showContextualActions([]);
  },

  _updateContextualActionLabel: function(aBtnNode, aVerb, aNoun, aQty) {
    // True if action modifies the noun for the grid (bookmark, top site, etc.),
    // causing the label to be pluralized by the number of selected items.
    let modifiesNoun = aBtnNode.getAttribute("modifies-noun") == "true";
    if (modifiesNoun && (!aNoun || isNaN(aQty))) {
      throw new Error("Appbar._updateContextualActionLabel: " +
                      "missing noun/quantity for " + aVerb);
    }

    let labelName = "contextAppbar." + aVerb + (modifiesNoun ? "." + aNoun : "");
    let label = Strings.browser.GetStringFromName(labelName);
    aBtnNode.label = modifiesNoun ? PluralForm.get(aQty, label) : label;
  },

  _onTileSelectionChanged: function _onTileSelectionChanged(aEvent){
    let activeTileset = aEvent.target;

    // deselect tiles in other tile groups
    if (this.activeTileset && this.activeTileset !== activeTileset) {
      this.activeTileset.clearSelection();
    }
    // keep track of which view is the target/scope for the contextual actions
    this.activeTileset = activeTileset;

    // ask the view for the list verbs/action-names it thinks are
    // appropriate for the tiles selected
    let contextActions = activeTileset.contextActions;
    let verbs = [v for (v of contextActions)];

    // fire event with these verbs as payload
    let event = document.createEvent("Events");
    event.actions = verbs;
    event.noun = activeTileset.contextNoun;
    event.qty = activeTileset.selectedItems.length;
    event.initEvent("MozContextActionsChange", true, false);
    Elements.contextappbar.dispatchEvent(event);

    if (verbs.length) {
      Elements.contextappbar.show(); // should be no-op if we're already showing
    } else {
      Elements.contextappbar.dismiss();
    }
  },

  _updatePinButton: function() {
    this.pinButton.checked = Browser.isSitePinned();
  },

  _updateStarButton: function() {
    Browser.isSiteStarredAsync(function (isStarred) {
      this.starButton.checked = isStarred;
    }.bind(this));
  },
};
