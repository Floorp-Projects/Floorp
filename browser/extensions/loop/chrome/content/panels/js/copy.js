/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components */
let { utils: Cu } = Components;

var loop = loop || {};
loop.copy = (mozL10n => {
  "use strict";

  /**
   * Copy panel view.
   */

  let CopyView = React.createClass({
    displayName: "CopyView",

    mixins: [React.addons.PureRenderMixin],

    getInitialState() {
      return { checked: false };
    },

    /**
     * Send a message to chrome/bootstrap to handle copy panel events.
     * @param {Boolean} accept True if the user clicked accept.
     */
    _dispatch(accept) {
      window.dispatchEvent(new CustomEvent("CopyPanelClick", {
        detail: {
          accept,
          stop: this.state.checked
        }
      }));
    },

    handleAccept() {
      this._dispatch(true);
    },

    handleCancel() {
      this._dispatch(false);
    },

    handleToggle() {
      this.setState({ checked: !this.state.checked });
    },

    render() {
      return React.createElement(
        "div",
        { className: "copy-content" },
        React.createElement(
          "div",
          { className: "copy-body" },
          React.createElement("img", { className: "copy-logo", src: "shared/img/helloicon.svg" }),
          React.createElement(
            "h1",
            { className: "copy-message" },
            mozL10n.get("copy_panel_message"),
            React.createElement(
              "label",
              { className: "copy-toggle-label" },
              React.createElement("input", { onChange: this.handleToggle, type: "checkbox" }),
              mozL10n.get("copy_panel_dont_show_again_label")
            )
          )
        ),
        React.createElement(
          "button",
          { className: "copy-button", onClick: this.handleCancel },
          mozL10n.get("copy_panel_cancel_button_label")
        ),
        React.createElement(
          "button",
          { className: "copy-button", onClick: this.handleAccept },
          mozL10n.get("copy_panel_accept_button_label")
        )
      );
    }
  });

  /**
   * Copy panel initialization.
   */
  function init() {
    // Wait for all LoopAPI message requests to complete before continuing init.
    let { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
    let requests = ["GetAllStrings", "GetLocale", "GetPluralRule"];
    return Promise.all(requests.map(name => new Promise(resolve => {
      LoopAPI.sendMessageToHandler({ name }, resolve);
    }))).then(results => {
      // Extract individual requested values to initialize l10n.
      let [stringBundle, locale, pluralRule] = results;
      mozL10n.initialize({
        getStrings(key) {
          if (!(key in stringBundle)) {
            console.error("No string found for key:", key);
          }
          return JSON.stringify({ textContent: stringBundle[key] || "" });
        },
        locale,
        pluralRule
      });

      React.render(React.createElement(CopyView, null), document.querySelector("#main"));

      document.documentElement.setAttribute("dir", mozL10n.language.direction);
      document.documentElement.setAttribute("lang", mozL10n.language.code);
    });
  }

  return {
    CopyView,
    init
  };
})(document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.copy.init);
