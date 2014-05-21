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
  },

  setUpInitialState: function () {
    this._send("GetState");
  },

  showPanel: function () {
    let panel = this._nodes.panel;
    let logo = this._nodes.logo;
    panel.openPopup(logo);
    logo.setAttribute("active", "true");
    panel.addEventListener("popuphidden", function onHidden() {
      panel.removeEventListener("popuphidden", onHidden);
      logo.removeAttribute("active");
    });
  },

  search: function (event) {
    event.preventDefault();
    let searchStr = this._nodes.text.value;
    if (this.currentEngineName && searchStr.length) {
      this._send("Search", {
        engineName: this.currentEngineName,
        searchString: searchStr,
        whence: "newtab",
      });
    }
  },

  manageEngines: function () {
    this._nodes.panel.hidePopup();
    this._send("ManageEngines");
  },

  setWidth: function (width) {
    this._nodes.form.style.width = width + "px";
    this._nodes.form.style.maxWidth = width + "px";
  },

  handleEvent: function (event) {
    this["on" + event.detail.type](event.detail.data);
  },

  onState: function (data) {
    this._makePanel(data.engines);
    this._setCurrentEngine(data.currentEngine);
    this._initWhenInitalStateReceived();
  },

  onCurrentEngine: function (engineName) {
    this._setCurrentEngine(engineName);
  },

  _nodeIDSuffixes: [
    "form",
    "logo",
    "manage",
    "panel",
    "text",
  ],

  _nodes: {},

  _initWhenInitalStateReceived: function () {
    this._nodes.form.addEventListener("submit", e => this.search(e));
    this._nodes.logo.addEventListener("click", e => this.showPanel());
    this._nodes.manage.addEventListener("click", e => this.manageEngines());
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

  _makePanel: function (engines) {
    let panel = this._nodes.panel;

    // Empty the panel except for the Manage Engines row.
    let i = 0;
    while (i < panel.childNodes.length) {
      let node = panel.childNodes[i];
      if (node != this._nodes.manage) {
        panel.removeChild(node);
      }
      else {
        i++;
      }
    }

    // Add all the engines.
    for (let engine of engines) {
      panel.insertBefore(this._makePanelEngine(panel, engine),
                         this._nodes.manage);
    }
  },

  _makePanelEngine: function (panel, engine) {
    let box = document.createElementNS(XUL_NAMESPACE, "hbox");
    box.className = "newtab-search-panel-engine";
    box.setAttribute("engine", engine.name);

    box.addEventListener("click", () => {
      this._send("SetCurrentEngine", engine.name);
      panel.hidePopup();
      this._nodes.text.focus();
    });

    let image = document.createElementNS(XUL_NAMESPACE, "image");
    if (engine.iconURI) {
      image.setAttribute("src", engine.iconURI);
    }
    box.appendChild(image);

    let label = document.createElementNS(XUL_NAMESPACE, "label");
    label.setAttribute("value", engine.name);
    box.appendChild(label);

    return box;
  },

  _setCurrentEngine: function (engine) {
    this.currentEngineName = engine.name;

    // Set the logo.
    let logoURI = window.devicePixelRatio == 2 ? engine.logo2xURI :
                  engine.logoURI || engine.logo2xURI;
    if (logoURI) {
      this._nodes.logo.hidden = false;
      this._nodes.logo.style.backgroundImage = "url(" + logoURI + ")";
      this._nodes.text.placeholder = "";
    }
    else {
      this._nodes.logo.hidden = true;
      this._nodes.text.placeholder = engine.name;
    }

    // Set the selected state of all the engines in the panel.
    for (let box of this._nodes.panel.childNodes) {
      if (box.getAttribute("engine") == engine.name) {
        box.setAttribute("selected", "true");
      }
      else {
        box.removeAttribute("selected");
      }
    }
  },
};
