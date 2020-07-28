/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Wrap in a block to prevent leaking to window scope.
{
  /**
   * A richlistbox popup custom element for for a browser search autocomplete
   * widget.
   */
  class MozSearchAutocompleteRichlistboxPopup extends MozElements.MozAutocompleteRichlistboxPopup {
    constructor() {
      super();

      this.addEventListener("popupshowing", event => {
        // First handle deciding if we are showing the reduced version of the
        // popup containing only the preferences button. We do this if the
        // glass icon has been clicked if the text field is empty.
        let searchbar = document.getElementById("searchbar");
        if (searchbar.hasAttribute("showonlysettings")) {
          searchbar.removeAttribute("showonlysettings");
          this.setAttribute("showonlysettings", "true");

          // Setting this with an xbl-inherited attribute gets overridden the
          // second time the user clicks the glass icon for some reason...
          this.richlistbox.collapsed = true;
        } else {
          this.removeAttribute("showonlysettings");
          // Uncollapse as long as we have a view which has >= 1 row.
          // The autocomplete binding itself will take care of uncollapsing later,
          // if we currently have no rows but end up having some in the future
          // when the search string changes
          this.richlistbox.collapsed = this.matchCount == 0;
        }

        // Show the current default engine in the top header of the panel.
        this.updateHeader().catch(Cu.reportError);

        this._oneOffButtons.addEventListener(
          "SelectedOneOffButtonChanged",
          this
        );
      });

      this.addEventListener("popuphiding", event => {
        this._oneOffButtons.removeEventListener(
          "SelectedOneOffButtonChanged",
          this
        );
      });

      /**
       * This handles clicks on the topmost "Foo Search" header in the
       * popup (hbox.search-panel-header]).
       */
      this.addEventListener("click", event => {
        if (event.button == 2) {
          // Ignore right clicks.
          return;
        }
        let button = event.originalTarget;
        let engine = button.parentNode.engine;
        if (!engine) {
          return;
        }
        this.oneOffButtons.handleSearchCommand(event, engine);
      });

      this._bundle = null;
    }

    static get inheritedAttributes() {
      return {
        ".search-panel-current-engine": "showonlysettings",
        ".searchbar-engine-image": "src",
      };
    }

    initialize() {
      super.initialize();
      this.initializeAttributeInheritance();

      this._searchOneOffsContainer = this.querySelector(".search-one-offs");
      this._searchbarEngine = this.querySelector(".search-panel-header");
      this._searchbarEngineName = this.querySelector(".searchbar-engine-name");
      this._oneOffButtons = new SearchOneOffs(this._searchOneOffsContainer);
    }

    get oneOffButtons() {
      if (!this._oneOffButtons) {
        this.initialize();
      }
      return this._oneOffButtons;
    }

    static get markup() {
      return `
      <hbox class="search-panel-header search-panel-current-engine">
        <image class="searchbar-engine-image"></image>
        <label class="searchbar-engine-name" flex="1" crop="end" role="presentation"></label>
      </hbox>
      <richlistbox class="autocomplete-richlistbox search-panel-tree" flex="1"></richlistbox>
      <hbox class="search-one-offs"></hbox>
    `;
    }

    get searchOneOffsContainer() {
      if (!this._searchOneOffsContainer) {
        this.initialize();
      }
      return this._searchOneOffsContainer;
    }

    get searchbarEngine() {
      if (!this._searchbarEngine) {
        this.initialize();
      }
      return this._searchbarEngine;
    }

    get searchbarEngineName() {
      if (!this._searchbarEngineName) {
        this.initialize();
      }
      return this._searchbarEngineName;
    }

    get bundle() {
      if (!this._bundle) {
        const kBundleURI = "chrome://browser/locale/search.properties";
        this._bundle = Services.strings.createBundle(kBundleURI);
      }
      return this._bundle;
    }

    openAutocompletePopup(aInput, aElement) {
      // initially the panel is hidden
      // to avoid impacting startup / new window performance
      aInput.popup.hidden = false;

      // this method is defined on the base binding
      this._openAutocompletePopup(aInput, aElement);
    }

    onPopupClick(aEvent) {
      // Ignore all right-clicks
      if (aEvent.button == 2) {
        return;
      }

      let searchBar = BrowserSearch.searchBar;
      let popupForSearchBar = searchBar && searchBar.textbox == this.mInput;
      if (popupForSearchBar) {
        searchBar.telemetrySearchDetails = {
          index: this.selectedIndex,
          kind: "mouse",
        };
      }

      // Check for unmodified left-click, and use default behavior
      if (
        aEvent.button == 0 &&
        !aEvent.shiftKey &&
        !aEvent.ctrlKey &&
        !aEvent.altKey &&
        !aEvent.metaKey
      ) {
        this.input.controller.handleEnter(true, aEvent);
        return;
      }

      // Check for middle-click or modified clicks on the search bar
      if (popupForSearchBar) {
        BrowserUsageTelemetry.recordSearchbarSelectedResultMethod(
          aEvent,
          this.selectedIndex
        );

        // Handle search bar popup clicks
        let search = this.input.controller.getValueAt(this.selectedIndex);

        // open the search results according to the clicking subtlety
        let where = whereToOpenLink(aEvent, false, true);
        let params = {};

        // But open ctrl/cmd clicks on autocomplete items in a new background tab.
        let modifier =
          AppConstants.platform == "macosx" ? aEvent.metaKey : aEvent.ctrlKey;
        if (
          where == "tab" &&
          aEvent instanceof MouseEvent &&
          (aEvent.button == 1 || modifier)
        ) {
          params.inBackground = true;
        }

        // leave the popup open for background tab loads
        if (!(where == "tab" && params.inBackground)) {
          // close the autocomplete popup and revert the entered search term
          this.closePopup();
          this.input.controller.handleEscape();
        }

        searchBar.doSearch(search, where, null, params);
        if (where == "tab" && params.inBackground) {
          searchBar.focus();
        } else {
          searchBar.value = search;
        }
      }
    }

    async updateHeader(engine) {
      if (!engine) {
        if (PrivateBrowsingUtils.isWindowPrivate(window)) {
          engine = await Services.search.getDefaultPrivate();
        } else {
          engine = await Services.search.getDefault();
        }
      }

      let uri = engine.iconURI;
      if (uri) {
        this.setAttribute("src", uri.spec);
      } else {
        // If the default has just been changed to a provider without icon,
        // avoid showing the icon of the previous default provider.
        this.removeAttribute("src");
      }

      let headerText = this.bundle.formatStringFromName("searchHeader", [
        engine.name,
      ]);
      this.searchbarEngineName.setAttribute("value", headerText);
      this.searchbarEngine.engine = engine;
    }

    /**
     * This is called when a one-off is clicked and when "search in new tab"
     * is selected from a one-off context menu.
     */
    /* eslint-disable-next-line valid-jsdoc */
    handleOneOffSearch(event, engine, where, params) {
      let searchbar = document.getElementById("searchbar");
      searchbar.handleSearchCommandWhere(event, engine, where, params);
    }

    /**
     * Passes DOM events for the popup to the _on_<event type> methods.
     * @param {Event} event
     *   DOM event from the <popup>.
     */
    handleEvent(event) {
      let methodName = "_on_" + event.type;
      if (methodName in this) {
        this[methodName](event);
      } else {
        throw new Error("Unrecognized UrlbarView event: " + event.type);
      }
    }
    _on_SelectedOneOffButtonChanged() {
      let engine =
        this.oneOffButtons.selectedButton &&
        this.oneOffButtons.selectedButton.engine;
      this.updateHeader(engine).catch(Cu.reportError);
    }
  }

  customElements.define(
    "search-autocomplete-richlistbox-popup",
    MozSearchAutocompleteRichlistboxPopup,
    {
      extends: "panel",
    }
  );
}
