/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  PureComponent,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const { hr } = dom;

loader.lazyGetter(this, "MenuItem", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuItem.js")
  );
});
loader.lazyGetter(this, "MenuList", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuList.js")
  );
});

loader.lazyRequireGetter(
  this,
  "openDocLink",
  "resource://devtools/client/shared/link.js",
  true
);
loader.lazyRequireGetter(
  this,
  "assert",
  "resource://devtools/shared/DevToolsUtils.js",
  true
);

const openDevToolsDocsLink = () => {
  openDocLink("https://firefox-source-docs.mozilla.org/devtools-user/");
};

const openCommunityLink = () => {
  openDocLink(
    "https://discourse.mozilla.org/c/devtools?utm_source=devtools&utm_medium=tabbar-menu"
  );
};

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
      // |hostTypes| but this is not always the case (e.g. for "browsertoolbox").
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

      // Apply a pseudo-locale to the Firefox UI. This is only available in the browser
      // toolbox. This value can be undefined, "accented", "bidi", "none".
      pseudoLocale: PropTypes.string,

      // Function to turn the options panel on / off.
      toggleOptions: PropTypes.func.isRequired,

      // Function to turn the split console on / off.
      toggleSplitConsole: PropTypes.func,

      // Function to turn the disable pop-up autohide behavior on / off.
      toggleNoAutohide: PropTypes.func,

      // Manage the pseudo-localization for the Firefox UI.
      // https://firefox-source-docs.mozilla.org/l10n/fluent/tutorial.html#manually-testing-ui-with-pseudolocalization
      disablePseudoLocale: PropTypes.func,
      enableAccentedPseudoLocale: PropTypes.func,
      enableBidiPseudoLocale: PropTypes.func,

      // Bug 1709191 - The help shortcut key is localized without Fluent, and still needs
      // to be migrated. This is the only remaining use of the legacy L10N object.
      // Everything else should prefer the Fluent API.
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
    // - The pseudo locale options being added after the Browser Toolbox is connected.
    // - The split console label changing between "Show Split Console" and "Hide
    //   Split Console".
    // - The "Show/Hide Split Console" entry being added removed or removed.
    //
    // The latter two cases are only likely to be noticed when "Disable pop-up
    // autohide" is active, but for completeness we handle them here.
    const didChange =
      typeof this.props.disableAutohide !== typeof prevProps.disableAutohide ||
      this.props.pseudoLocale !== prevProps.pseudoLocale ||
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
      // This is more verbose than it needs to be but lets us easily search for
      // l10n entities.
      let l10nID;
      switch (hostType.position) {
        case "window":
          l10nID = "toolbox-meatball-menu-dock-separate-window-label";
          break;

        case "bottom":
          l10nID = "toolbox-meatball-menu-dock-bottom-label";
          break;

        case "left":
          l10nID = "toolbox-meatball-menu-dock-left-label";
          break;

        case "right":
          l10nID = "toolbox-meatball-menu-dock-right-label";
          break;

        default:
          assert(false, `Unexpected hostType.position: ${hostType.position}`);
          break;
      }

      items.push(
        MenuItem({
          id: `toolbox-meatball-menu-dock-${hostType.position}`,
          key: `dock-${hostType.position}`,
          l10nID,
          onClick: hostType.switchHost,
          checked: hostType.position === this.props.currentHostType,
          className: "iconic",
        })
      );
    }

    if (items.length) {
      items.push(hr({ key: "dock-separator" }));
    }

    // Split console
    if (this.props.currentToolId !== "webconsole") {
      const l10nID = this.props.isSplitConsoleActive
        ? "toolbox-meatball-menu-hideconsole-label"
        : "toolbox-meatball-menu-splitconsole-label";
      items.push(
        MenuItem({
          id: "toolbox-meatball-menu-splitconsole",
          key: "splitconsole",
          l10nID,
          accelerator: "Esc",
          onClick: this.props.toggleSplitConsole,
          className: "iconic",
        })
      );
    }

    // Settings
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-settings",
        key: "settings",
        l10nID: "toolbox-meatball-menu-settings-label",
        // Bug 1709191 - The help key is localized without Fluent, and still needs to
        // be migrated.
        accelerator: this.props.L10N.getStr("toolbox.help.key"),
        onClick: this.props.toggleOptions,
        className: "iconic",
      })
    );

    if (
      typeof this.props.disableAutohide !== "undefined" ||
      typeof this.props.pseudoLocale !== "undefined"
    ) {
      items.push(hr({ key: "docs-separator-1" }));
    }

    // Disable pop-up autohide
    //
    // If |disableAutohide| is undefined, it means this feature is not available
    // in this context.
    if (typeof this.props.disableAutohide !== "undefined") {
      items.push(
        MenuItem({
          id: "toolbox-meatball-menu-noautohide",
          key: "noautohide",
          l10nID: "toolbox-meatball-menu-noautohide-label",
          type: "checkbox",
          checked: this.props.disableAutohide,
          onClick: this.props.toggleNoAutohide,
          className: "iconic",
        })
      );
    }

    // Pseudo-locales.
    if (typeof this.props.pseudoLocale !== "undefined") {
      const {
        pseudoLocale,
        enableAccentedPseudoLocale,
        enableBidiPseudoLocale,
        disablePseudoLocale,
      } = this.props;
      items.push(
        MenuItem({
          id: "toolbox-meatball-menu-pseudo-locale-accented",
          key: "pseudo-locale-accented",
          l10nID: "toolbox-meatball-menu-pseudo-locale-accented",
          type: "checkbox",
          checked: pseudoLocale === "accented",
          onClick:
            pseudoLocale === "accented"
              ? disablePseudoLocale
              : enableAccentedPseudoLocale,
          className: "iconic",
        }),
        MenuItem({
          id: "toolbox-meatball-menu-pseudo-locale-bidi",
          key: "pseudo-locale-bidi",
          l10nID: "toolbox-meatball-menu-pseudo-locale-bidi",
          type: "checkbox",
          checked: pseudoLocale === "bidi",
          onClick:
            pseudoLocale === "bidi"
              ? disablePseudoLocale
              : enableBidiPseudoLocale,
          className: "iconic",
        })
      );
    }

    items.push(hr({ key: "docs-separator-2" }));

    // Getting started
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-documentation",
        key: "documentation",
        l10nID: "toolbox-meatball-menu-documentation-label",
        onClick: openDevToolsDocsLink,
      })
    );

    // Give feedback
    items.push(
      MenuItem({
        id: "toolbox-meatball-menu-community",
        key: "community",
        l10nID: "toolbox-meatball-menu-community-label",
        onClick: openCommunityLink,
      })
    );

    return MenuList({ id: "toolbox-meatball-menu" }, items);
  }
}

module.exports = MeatballMenu;
