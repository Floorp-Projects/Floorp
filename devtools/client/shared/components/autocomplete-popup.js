/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

module.exports = createClass({
  displayName: "AutocompletePopup",

  propTypes: {
    list: PropTypes.array.isRequired,
    filter: PropTypes.string.isRequired,
    onItemSelected: PropTypes.func.isRequired,
  },

  getInitialState() {
    return this.computeState(this.props);
  },

  componentWillReceiveProps(nextProps) {
    if (this.props.filter === nextProps.filter) {
      return;
    }
    this.setState(this.computeState(nextProps));
  },

  componentDidUpdate() {
    if (this.refs.selected) {
      this.refs.selected.scrollIntoView(false);
    }
  },

  computeState({ filter, list }) {
    let filteredList = list.filter((item) => {
      return item.toLowerCase().startsWith(filter.toLowerCase())
        && item.toLowerCase() !== filter.toLowerCase();
    }).sort();
    let selectedIndex = filteredList.length == 1 ? 0 : -1;

    return { filteredList, selectedIndex };
  },

  /**
   * Use this method to select the top-most item
   * This method is public, called outside of the autocomplete-popup component.
   */
  jumpToTop() {
    this.setState({ selectedIndex: 0 });
  },

  /**
   * Use this method to select the bottom-most item
   * This method is public.
   */
  jumpToBottom() {
    let selectedIndex = this.state.filteredList.length - 1;
    this.setState({ selectedIndex });
  },

  /**
   * Increment the selected index with the provided increment value. Will cycle to the
   * beginning/end of the list if the index exceeds the list boundaries.
   * This method is public.
   *
   * @param {number} increment - No. of hops in the direction
   */
  jumpBy(increment = 1) {
    let { filteredList, selectedIndex } = this.state;
    let nextIndex = selectedIndex + increment;
    if (increment > 0) {
      // Positive cycling
      nextIndex = nextIndex > filteredList.length - 1 ? 0 : nextIndex;
    } else if (increment < 0) {
      // Inverse cycling
      nextIndex = nextIndex < 0 ? filteredList.length - 1 : nextIndex;
    }
    this.setState({selectedIndex: nextIndex});
  },

  /**
   * Submit the currently selected item to the onItemSelected callback
   * This method is public.
   */
  select() {
    if (this.refs.selected) {
      this.props.onItemSelected(this.refs.selected.textContent);
    }
  },

  onMouseDown(e) {
    e.preventDefault();
    this.setState({ selectedIndex: Number(e.target.dataset.index) }, this.select);
  },

  render() {
    let { filteredList } = this.state;

    return filteredList.length > 0 && dom.div(
      { className: "devtools-autocomplete-popup devtools-monospace" },
      dom.ul(
        { className: "devtools-autocomplete-listbox" },
        filteredList.map((item, i) => {
          let isSelected = this.state.selectedIndex == i;
          let itemClassList = ["autocomplete-item"];

          if (isSelected) {
            itemClassList.push("autocomplete-selected");
          }
          return dom.li({
            key: i,
            "data-index": i,
            className: itemClassList.join(" "),
            ref: isSelected ? "selected" : null,
            onMouseDown: this.onMouseDown,
          }, item);
        })
      )
    );
  }
});
