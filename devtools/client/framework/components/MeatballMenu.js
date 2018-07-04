/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { createFactory } = require("devtools/client/shared/vendor/react");
const MenuItem = createFactory(
  require("devtools/client/shared/components/menu/MenuItem")
);
const MenuList = createFactory(
  require("devtools/client/shared/components/menu/MenuList")
);
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { hr } = dom;
const { openDocLink } = require("devtools/client/shared/link");

class MeatballMenu extends PureComponent {
  static get propTypes() {
    return {
      // The id of the currently selected tool, e.g. "inspector"
      currentToolId: PropTypes.string,

      // List of possible docking options.
      hostTypes: PropTypes.arrayOf(
        PropTypes.shape({
          position: PropTypes.string.isRequired,
          switchHost: PropTypes.func.isRequired,
        })
      ),

      // Current docking type. Typically one of the position values in
      // |hostTypes| but this is not always the case (e.g. when it is "custom").
      currentHostType: PropTypes.string,

      // Is the split console currently visible?
      isSplitConsoleActive: PropTypes.bool,

      // Are we disabling the behavior where pop-ups are automatically closed
      // when clicking outside them?
      //
      // This is a tri-state value that may be true/false or undefined where
      // undefined means that the option is not relevant in this context
      // (i.e. we're not in a browser toolbox).
      disableAutohide: PropTypes.bool,

      // Function to turn the options panel on / off.
      toggleOptions: PropTypes.func.isRequired,

      // Function to turn the split console on / off.
      toggleSplitConsole: PropTypes.func,

      // Function to turn the disable pop-up autohide behavior on / off.
      toggleNoAutohide: PropTypes.func,

      // Localization interface.
      L10N: PropTypes.object.isRequired,

      // Callback function that will be invoked any time the component contents
      // update in such a way that its bounding box might change.
      onResize: PropTypes.func,
    };
  }

  componentDidUpdate(prevProps) {
    if (!this.props.onResize) {
      return;
    }

    // We are only expecting the following kinds of dynamic changes when a popup
    // is showing:
    //
    // - The "Disable pop-up autohide" menu item being added after the Browser
    //   Toolbox is connected.
    // - The split console label changing between "Show split console" and "Hide
    //   split console".
    // - The "Show/Hide split console" entry being added removed or removed.
    //
    // The latter two cases are only likely to be noticed when "Disable pop-up
    // autohide" is active, but for completeness we handle them here.
    const didChange =
      typeof this.props.disableAutohide !== typeof prevProps.disableAutohide ||
      this.props.currentToolId !== prevProps.currentToolId ||
      this.props.isSplitConsoleActive !== prevProps.isSplitConsoleActive;

    if (didChange) {
      this.props.onResize();
    }
  }

  render() {
    const items = [];

    // Dock options
    for (const hostType of this.props.hostTypes) {
      const l10nkey =
        hostType.position === "window" ? "separateWindow" : hostType.position;
      items.push(
        MenuItem({
          id: `toolbox-meatball-menu-dock-${hostType.position}`,
          label: this.props.L10N.getStr(
            `toolbox.meatballMenu.dock.${l10nkey}.label`
          ),
          onClick: () => hostType.switchHost(),
          checked: hostType.position === this.props.currentHostType,
          className: "iconic",
        })
      );
    }

    if (items.length) {
      items.push(hr());
    }

    // Split console
    if (this.props.currentToolId !== "webconsole") {
      items.push(
        MenuItem({
          id: "toolbox-meatball-menu-splitconsole",
          label: this.props.L10N.getStr(
            `toolbox.meatballMenu.${
              this.props.isSplitConsoleActive ? "hideconsole" : "splitconsole"
            }.label`
          ),
          accelerator: "Esc",
          onClick: this.props.toggleSplitConsole,
          className: "iconic",
        })
      );
    }

    // Disable pop-up autohide
    //
    // If |disableAutohide| is undefined, it means this feature is not available
    // in this context.
    if (typeof this.props.disableAutohide !== "undefined") {
      items.push(
        MenuItem({
          id: "toolbox-meatball-menu-noautohide",
          label: this.props.L10N.getStr(
            "toolbox.meatballMenu.noautohide.label"
          ),
          type: "checkbox",
          checked: this.props.disableAutohide,
          onClick: this.props.toggleNoAutohide,
          className: "iconic",
        })
      );
    }

    // Settings
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-settings",
        label: this.props.L10N.getStr("toolbox.meatballMenu.settings.label"),
        accelerator: this.props.L10N.getStr("toolbox.help.key"),
        onClick: () => this.props.toggleOptions(),
        className: "iconic",
      })
    );

    items.push(hr());

    // Getting started
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-documentation",
        label: this.props.L10N.getStr(
          "toolbox.meatballMenu.documentation.label"
        ),
        onClick: () => {
          openDocLink(
            "https://developer.mozilla.org/docs/Tools?utm_source=devtools&utm_medium=tabbar-menu"
          );
        },
      })
    );

    // Give feedback
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-community",
        label: this.props.L10N.getStr("toolbox.meatballMenu.community.label"),
        onClick: () => {
          openDocLink(
            "https://discourse.mozilla.org/c/devtools?utm_source=devtools&utm_medium=tabbar-menu"
          );
        },
      })
    );

    return MenuList({ id: "toolbox-meatball-menu" }, items);
  }
}

module.exports = MeatballMenu;
