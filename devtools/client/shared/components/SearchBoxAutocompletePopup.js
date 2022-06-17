/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class SearchBoxAutocompletePopup extends Component {
  static get propTypes() {
    return {
      /**
       * autocompleteProvider takes search-box's entire input text as `filter` argument
       * ie. "is:cached pr"
       * returned value is array of objects like below
       * [{value: "is:cached protocol", displayValue: "protocol"}[, ...]]
       * `value` is used to update the search-box input box for given item
       * `displayValue` is used to render the autocomplete list
       */
      autocompleteProvider: PropTypes.func.isRequired,
      filter: PropTypes.string.isRequired,
      onItemSelected: PropTypes.func.isRequired,
    };
  }

  constructor(props, context) {
    super(props, context);
    this.state = this.computeState(props);
    this.computeState = this.computeState.bind(this);
    this.jumpToTop = this.jumpToTop.bind(this);
    this.jumpToBottom = this.jumpToBottom.bind(this);
    this.jumpBy = this.jumpBy.bind(this);
    this.select = this.select.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    if (this.props.filter === nextProps.filter) {
      return;
    }
    this.setState(this.computeState(nextProps));
  }

  componentDidUpdate() {
    if (this.refs.selected) {
      this.refs.selected.scrollIntoView(false);
    }
  }

  computeState({ autocompleteProvider, filter }) {
    const list = autocompleteProvider(filter);
    const selectedIndex = list.length > 0 ? 0 : -1;

    return { list, selectedIndex };
  }

  /**
   * Use this method to select the top-most item
   * This method is public, called outside of the autocomplete-popup component.
   */
  jumpToTop() {
    this.setState({ selectedIndex: 0 });
  }

  /**
   * Use this method to select the bottom-most item
   * This method is public.
   */
  jumpToBottom() {
    this.setState({ selectedIndex: this.state.list.length - 1 });
  }

  /**
   * Increment the selected index with the provided increment value. Will cycle to the
   * beginning/end of the list if the index exceeds the list boundaries.
   * This method is public.
   *
   * @param {number} increment - No. of hops in the direction
   */
  jumpBy(increment = 1) {
    const { list, selectedIndex } = this.state;
    let nextIndex = selectedIndex + increment;
    if (increment > 0) {
      // Positive cycling
      nextIndex = nextIndex > list.length - 1 ? 0 : nextIndex;
    } else if (increment < 0) {
      // Inverse cycling
      nextIndex = nextIndex < 0 ? list.length - 1 : nextIndex;
    }
    this.setState({ selectedIndex: nextIndex });
  }

  /**
   * Submit the currently selected item to the onItemSelected callback
   * This method is public.
   */
  select() {
    if (this.refs.selected) {
      this.props.onItemSelected(this.refs.selected.dataset.value);
    }
  }

  onMouseDown(e) {
    e.preventDefault();
    this.setState(
      { selectedIndex: Number(e.target.dataset.index) },
      this.select
    );
  }

  render() {
    const { list } = this.state;

    return (
      list.length > 0 &&
      dom.div(
        { className: "devtools-autocomplete-popup devtools-monospace" },
        dom.ul(
          { className: "devtools-autocomplete-listbox" },
          list.map((item, i) => {
            const isSelected = this.state.selectedIndex == i;
            const itemClassList = ["autocomplete-item"];

            if (isSelected) {
              itemClassList.push("autocomplete-selected");
            }
            return dom.li(
              {
                key: i,
                "data-index": i,
                "data-value": item.value,
                className: itemClassList.join(" "),
                ref: isSelected ? "selected" : null,
                onMouseDown: this.onMouseDown,
              },
              item.displayValue
            );
          })
        )
      )
    );
  }
}

module.exports = SearchBoxAutocompletePopup;
