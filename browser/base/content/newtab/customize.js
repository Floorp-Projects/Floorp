#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

let gCustomize = {
  _nodeIDSuffixes: [
    "blank",
    "button",
    "classic",
    "enhanced",
    "panel",
  ],

  _nodes: {},

  init: function() {
    for (let idSuffix of this._nodeIDSuffixes) {
      this._nodes[idSuffix] = document.getElementById("newtab-customize-" + idSuffix);
    }

    this._nodes.button.setAttribute("title", newTabString("customize.title"));
    ["enhanced", "classic", "blank"].forEach(name => {
      this._nodes[name].firstChild.textContent = newTabString("customize." + name);
    });

    this._nodes.button.addEventListener("click", e => this.showPanel());
    this._nodes.blank.addEventListener("click", e => {
      gAllPages.enabled = false;
    });
    this._nodes.classic.addEventListener("click", e => {
      gAllPages.enabled = true;
      gAllPages.enhanced = false;
    });
    this._nodes.enhanced.addEventListener("click", e => {
      gAllPages.enabled = true;
      gAllPages.enhanced = true;
    });

    this.updateSelected();
  },

  showPanel: function() {
    if (!DirectoryLinksProvider.enabled) {
      gAllPages.enabled = !gAllPages.enabled;
      return;
    }

    let nodes = this._nodes;
    let {button, panel} = nodes;
    if (button.hasAttribute("active")) {
      return Promise.resolve(nodes);
    }

    panel.openPopup(button);
    button.setAttribute("active", true);
    panel.addEventListener("popuphidden", function onHidden() {
      panel.removeEventListener("popuphidden", onHidden);
      button.removeAttribute("active");
    });

    return new Promise(resolve => {
      panel.addEventListener("popupshown", function onShown() {
        panel.removeEventListener("popupshown", onShown);
        resolve(nodes);
      });
    });
  },

  updateSelected: function() {
    let {enabled, enhanced} = gAllPages;

    if (!DirectoryLinksProvider.enabled) {
      this._nodes.button.setAttribute("title", newTabString(enabled ? "hide" : "show"));
      return;
    }

    let selected = enabled ? enhanced ? "enhanced" : "classic" : "blank";
    ["enhanced", "classic", "blank"].forEach(id => {
      let node = this._nodes[id];
      if (id == selected) {
        node.setAttribute("selected", true);
      }
      else {
        node.removeAttribute("selected");
      }
    });
  },
};
