/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A list of menu items.
//
// This component provides keyboard navigation amongst any focusable
// children.

const {
  Children,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const { focusableSelector } = require("devtools/client/shared/focus");

class MenuList extends PureComponent {
  static get propTypes() {
    return {
      // ID to assign to the list container.
      id: PropTypes.string,

      // Children of the list.
      children: PropTypes.any,

      // Called whenever there is a change to the hovered or selected child.
      // The callback is passed the ID of the highlighted child or null if no
      // child is highlighted.
      onHighlightedChildChange: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    this.onKeyDown = this.onKeyDown.bind(this);
    this.onMouseOverOrFocus = this.onMouseOverOrFocus.bind(this);
    this.onMouseOutOrBlur = this.onMouseOutOrBlur.bind(this);
    this.notifyHighlightedChildChange = this.notifyHighlightedChildChange.bind(
      this
    );

    this.setWrapperRef = element => {
      this.wrapperRef = element;
    };
  }

  onMouseOverOrFocus(e) {
    this.notifyHighlightedChildChange(e.target.id);
  }

  onMouseOutOrBlur(e) {
    const hoveredElem = this.wrapperRef.querySelector(":hover");
    if (!hoveredElem) {
      this.notifyHighlightedChildChange(null);
    }
  }

  notifyHighlightedChildChange(id) {
    if (this.props.onHighlightedChildChange) {
      this.props.onHighlightedChildChange(id);
    }
  }

  onKeyDown(e) {
    // Check if the focus is in the list.
    if (
      !this.wrapperRef ||
      !this.wrapperRef.contains(e.target.ownerDocument.activeElement)
    ) {
      return;
    }

    const getTabList = () =>
      Array.from(this.wrapperRef.querySelectorAll(focusableSelector));

    switch (e.key) {
      case "Tab":
      case "ArrowUp":
      case "ArrowDown":
        {
          const tabList = getTabList();
          const currentElement = e.target.ownerDocument.activeElement;
          const currentIndex = tabList.indexOf(currentElement);
          if (currentIndex !== -1) {
            let nextIndex;
            if (e.key === "ArrowDown" || (e.key === "Tab" && !e.shiftKey)) {
              nextIndex =
                currentIndex === tabList.length - 1 ? 0 : currentIndex + 1;
            } else {
              nextIndex =
                currentIndex === 0 ? tabList.length - 1 : currentIndex - 1;
            }
            tabList[nextIndex].focus();
            e.preventDefault();
          }
        }
        break;

      case "Home":
        {
          const firstItem = this.wrapperRef.querySelector(focusableSelector);
          if (firstItem) {
            firstItem.focus();
            e.preventDefault();
          }
        }
        break;

      case "End":
        {
          const tabList = getTabList();
          if (tabList.length) {
            tabList[tabList.length - 1].focus();
            e.preventDefault();
          }
        }
        break;
    }
  }

  render() {
    const attr = {
      role: "menu",
      ref: this.setWrapperRef,
      onKeyDown: this.onKeyDown,
      onMouseOver: this.onMouseOverOrFocus,
      onMouseOut: this.onMouseOutOrBlur,
      onFocus: this.onMouseOverOrFocus,
      onBlur: this.onMouseOutOrBlur,
      className: "menu-standard-padding",
    };

    if (this.props.id) {
      attr.id = this.props.id;
    }

    // Add padding for checkbox image if necessary.
    let hasCheckbox = false;
    Children.forEach(this.props.children, child => {
      if (typeof child.props.checked !== "undefined") {
        hasCheckbox = true;
      }
    });
    if (hasCheckbox) {
      attr.className = "checkbox-container menu-standard-padding";
    }

    return div(attr, this.props.children);
  }
}

module.exports = MenuList;
