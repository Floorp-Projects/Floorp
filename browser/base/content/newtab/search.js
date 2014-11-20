#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

let gSearch = {

  currentEngineName: null,

  init: function () {
    for (let idSuffix of this._nodeIDSuffixes) {
      this._nodes[idSuffix] =
        document.getElementById("newtab-search-" + idSuffix);
    }

    window.addEventListener("ContentSearchService", this);
    this._send("GetState");
  },

  showPanel: function () {
    let panel = this._nodes.panel;
    let icon = this._nodes.icon;
    panel.openPopup(icon);
    icon.setAttribute("active", "true");
    panel.addEventListener("popuphidden", function onHidden() {
      panel.removeEventListener("popuphidden", onHidden);
      icon.removeAttribute("active");
    });
  },

  search: function (event) {
    if (event) {
      event.preventDefault();
    }
    let searchStr = this._nodes.text.value;
    if (this.currentEngineName && searchStr.length) {
      this._send("Search", {
        engineName: this.currentEngineName,
        searchString: searchStr,
        whence: "newtab",
      });
    }
    this._suggestionController.addInputValueToFormHistory();
  },

  manageEngines: function () {
    this._nodes.panel.hidePopup();
    this._send("ManageEngines");
  },

  handleEvent: function (event) {
    let methodName = "on" + event.detail.type;
    if (this.hasOwnProperty(methodName)) {
      this[methodName](event.detail.data);
    }
  },

  onState: function (data) {
    this._setCurrentEngine(data.currentEngine);
    this._initWhenInitalStateReceived();
  },

  onCurrentState: function (data) {
    if (this._initialStateReceived) {
      this._setCurrentEngine(data.currentEngine);
    }
  },

  onCurrentEngine: function (engineName) {
    if (this._initialStateReceived) {
      this._nodes.panel.hidePopup();
      this._setCurrentEngine(engineName);
    }
  },

  onFocusInput: function () {
    this._nodes.text.focus();
  },

  _nodeIDSuffixes: [
    "form",
    "icon",
    "manage",
    "panel",
    "text",
  ],

  _nodes: {},

  _initWhenInitalStateReceived: function () {
    this._nodes.form.addEventListener("submit", e => this.search(e));
    this._nodes.icon.addEventListener("click", e => this.showPanel());
    this._nodes.manage.addEventListener("click", e => this.manageEngines());
    this._initialStateReceived = true;
    this._initWhenInitalStateReceived = function () {};
  },

  _send: function (type, data=null) {
    window.dispatchEvent(new CustomEvent("ContentSearchClient", {
      detail: {
        type: type,
        data: data,
      },
    }));
  },

  _setCurrentEngine: function (engine) {
    this.currentEngineName = engine.name;

    // Set up the suggestion controller.
    if (!this._suggestionController) {
      let parent = document.getElementById("newtab-scrollbox");
      this._suggestionController =
        new SearchSuggestionUIController(this._nodes.text, parent,
                                         () => this.search());
    }
    this._suggestionController.engineName = engine.name;
  },
};
