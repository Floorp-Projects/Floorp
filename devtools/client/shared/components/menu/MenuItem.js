/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A command in a menu.

const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { button, li, span } = dom;

class MenuItem extends PureComponent {
  static get propTypes() {
    return {
      // An optional keyboard shortcut to display next to the item.
      // (This does not actually register the event listener for the key.)
      accelerator: PropTypes.string,

      // A tri-state value that may be true/false if item should be checkable,
      // and undefined otherwise.
      checked: PropTypes.bool,

      // Any additional classes to assign to the button specified as
      // a space-separated string.
      className: PropTypes.string,

      // A disabled state of the menu item.
      disabled: PropTypes.bool,

      // URL of the icon to associate with the MenuItem. (Optional)
      //
      //   e.g. chrome://devtools/skim/image/foo.svg
      //
      // This may also be set in CSS using the --menuitem-icon-image variable.
      // Note that in this case, the variable should specify the CSS <image> to
      // use, not simply the URL (e.g.
      // "url(chrome://devtools/skim/image/foo.svg)").
      icon: PropTypes.string,

      // An optional ID to be assigned to the item.
      id: PropTypes.string,

      // The item label.
      label: PropTypes.string.isRequired,

      // An optional callback to be invoked when the item is selected.
      onClick: PropTypes.func,

      // Optional menu item role override. Use this property with a value
      // "menuitemradio" if the menu item is a radio.
      role: PropTypes.string,

      // An optional text for the item tooltip.
      tooltip: PropTypes.string,
    };
  }

  /**
   * Use this as a fallback `icon` prop if your MenuList contains MenuItems
   * with or without icon in order to keep all MenuItems aligned.
   */
  static get DUMMY_ICON() {
    return `data:image/svg+xml,${encodeURIComponent(
      '<svg height="16" width="16"></svg>'
    )}`;
  }

  constructor(props) {
    super(props);
    this.labelRef = createRef();
  }

  componentDidMount() {
    if (!this.labelRef.current) {
      return;
    }

    // Pre-fetch any backgrounds specified for the item.
    const win = this.labelRef.current.ownerDocument.defaultView;
    this.preloadCallback = win.requestIdleCallback(() => {
      this.preloadCallback = null;
      if (!this.labelRef.current) {
        return;
      }

      const backgrounds = win
        .getComputedStyle(this.labelRef.current, ":before")
        .getCSSImageURLs("background-image");
      for (const background of backgrounds) {
        const image = new Image();
        image.src = background;
      }
    });
  }

  componentWillUnmount() {
    if (!this.labelRef.current || !this.preloadCallback) {
      return;
    }

    const win = this.labelRef.current.ownerDocument.defaultView;
    if (win) {
      win.cancelIdleCallback(this.preloadCallback);
    }
    this.preloadCallback = null;
  }

  render() {
    const attr = {
      className: "command",
    };

    if (this.props.id) {
      attr.id = this.props.id;
    }

    if (this.props.className) {
      attr.className += " " + this.props.className;
    }

    if (this.props.icon) {
      attr.className += " iconic";
      attr.style = { "--menuitem-icon-image": "url(" + this.props.icon + ")" };
    }

    if (this.props.onClick) {
      attr.onClick = this.props.onClick;
    }

    if (this.props.tooltip) {
      attr.title = this.props.tooltip;
    }

    if (this.props.disabled) {
      attr.disabled = this.props.disabled;
    }

    if (this.props.role) {
      attr.role = this.props.role;
    } else if (typeof this.props.checked !== "undefined") {
      attr.role = "menuitemcheckbox";
    } else {
      attr.role = "menuitem";
    }

    if (this.props.checked) {
      attr["aria-checked"] = true;
    }

    const textLabel = span(
      { key: "label", className: "label", ref: this.labelRef },
      this.props.label
    );
    const children = [textLabel];

    if (typeof this.props.accelerator !== "undefined") {
      const acceleratorLabel = span(
        { key: "accelerator", className: "accelerator" },
        this.props.accelerator
      );
      children.push(acceleratorLabel);
    }

    return li(
      {
        className: "menuitem",
        role: "presentation",
      },
      button(attr, children)
    );
  }
}

module.exports = MenuItem;
