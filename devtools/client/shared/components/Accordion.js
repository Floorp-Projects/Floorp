/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { Component, cloneElement } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { ul, li, h2, div } = require("devtools/client/shared/vendor/react-dom-factories");

class Accordion extends Component {
  static get propTypes() {
    return {
      // A list of all items to be rendered using an Accordion component.
      items: PropTypes.arrayOf(PropTypes.shape({
        component: PropTypes.object,
        componentProps: PropTypes.object,
        buttons: PropTypes.arrayOf(PropTypes.object),
        className: PropTypes.string,
        header: PropTypes.string.isRequired,
        labelledby: PropTypes.string.isRequired,
        onToggle: PropTypes.func,
        opened: PropTypes.bool.isRequired,
      })).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      opened: Object.assign({}, props.items.map(item => item.opened)),
    };
  }

  /**
   * Expand or collapse an accordian list item.
   * @param  {Number} i
   *         Index of the item to be collapsed/expanded.
   */
  handleHeaderClick(i) {
    const { opened } = this.state;
    this.setState({
      opened: {
        ...opened,
        [i]: !opened[i],
      },
    });

    const item = this.props.items[i];
    if (item.onToggle) {
      item.onToggle(!opened[i]);
    }
  }

  /**
   * Expand or collapse an accordian list item with keyboard.
   * @param  {Event} e
   *         Keyboard event.
   * @param  {Number} i
   *         Index of the item to be collapsed/expanded.
   */
  onHandleHeaderKeyDown(e, i) {
    if (e && (e.key === " " || e.key === "Enter")) {
      this.handleHeaderClick(i);
    }
  }

  renderContainer(item, i) {
    const { buttons, className, component, componentProps, labelledby, header } = item;
    const opened = this.state.opened[i];

    return (
      li(
        {
          className,
          "aria-labelledby": labelledby,
          key: labelledby,
        },
        h2(
          {
            className: "accordion-header",
            id: labelledby,
            tabIndex: 0,
            "aria-expanded": opened,
            onKeyDown: e => this.onHandleHeaderKeyDown(e, i),
            onClick: () => this.handleHeaderClick(i),
          },
          div(
            {
              className: `arrow theme-twisty${opened ? " open" : ""}`,
              role: "presentation",
            }
          ),
          header,
          buttons && div(
            {
              className: "header-buttons",
              role: "presentation",
            },
            buttons
          ),
        ),
        opened && div(
          {
            className: "accordion-content",
            role: "presentation",
          },
          cloneElement(component, componentProps || {})
        )
      )
    );
  }

  render() {
    return (
      ul({
        className: "accordion",
        tabIndex: -1,
      },
        this.props.items.map((item, i) => this.renderContainer(item, i)))
    );
  }
}

module.exports = Accordion;
