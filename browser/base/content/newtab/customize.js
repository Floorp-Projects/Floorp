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
    "overlay",
    "learn"
  ],

  _nodes: {},

  init: function() {
    for (let idSuffix of this._nodeIDSuffixes) {
      this._nodes[idSuffix] = document.getElementById("newtab-customize-" + idSuffix);
    }

    this._nodes.button.addEventListener("click", e => this.showPanel());
    this._nodes.blank.addEventListener("click", e => {
      gAllPages.enabled = false;
    });
    this._nodes.classic.addEventListener("click", e => {
      gAllPages.enabled = true;

      if (this._nodes.enhanced.getAttribute("selected")) {
        gAllPages.enhanced = true;
      } else {
        gAllPages.enhanced = false;
      }
    });
    this._nodes.enhanced.addEventListener("click", e => {
      if (!gAllPages.enabled) {
        gAllPages.enabled = true;
        return;
      }
      gAllPages.enhanced = !gAllPages.enhanced;
    });
    this._nodes.learn.addEventListener("click", e => {
      window.open(TILES_INTRO_LINK,'new_window');
      this._onHidden();
    });

    this.updateSelected();
  },

  _onHidden: function() {
    let nodes = gCustomize._nodes;
    nodes.overlay.addEventListener("transitionend", function onTransitionEnd() {
      nodes.overlay.removeEventListener("transitionend", onTransitionEnd);
      nodes.overlay.style.display = "none";
    });
    nodes.overlay.style.opacity = 0;
    nodes.panel.removeEventListener("popuphidden", gCustomize._onHidden);
    nodes.panel.hidden = true;
    nodes.button.removeAttribute("active");
  },

  showPanel: function() {
    this._nodes.overlay.style.display = "block";
    setTimeout(() => {
      // Wait for display update to take place, then animate.
      this._nodes.overlay.style.opacity = 0.8;
    }, 0);

    let nodes = this._nodes;
    let {button, panel} = nodes;
    if (button.hasAttribute("active")) {
      return Promise.resolve(nodes);
    }

    panel.hidden = false;
    panel.openPopup(button);
    button.setAttribute("active", true);
    panel.addEventListener("popuphidden", this._onHidden);

    return new Promise(resolve => {
      panel.addEventListener("popupshown", function onShown() {
        panel.removeEventListener("popupshown", onShown);
        resolve(nodes);
      });
    });
  },

  updateSelected: function() {
    let {enabled, enhanced} = gAllPages;
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
    if (selected == "enhanced") {
      // If enhanced is selected, so is classic (since enhanced is a subitem of classic)
      this._nodes.classic.setAttribute("selected", true);
    }
  },
};
