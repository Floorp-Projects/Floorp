/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div, ul, li, span, h2, button } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import { score as fuzzaldrinScore } from "fuzzaldrin-plus";

import { containsPosition, positionAfter } from "../../utils/ast";
import { createLocation } from "../../utils/location";

import actions from "../../actions";
import {
  getSelectedLocation,
  getCursorPosition,
  getSelectedSourceTextContent,
} from "../../selectors";

import OutlineFilter from "./OutlineFilter";
import "./Outline.css";
import PreviewFunction from "../shared/PreviewFunction";

import { isFulfilled } from "../../utils/async-value";

const classnames = require("devtools/client/shared/classnames.js");

// Set higher to make the fuzzaldrin filter more specific
const FUZZALDRIN_FILTER_THRESHOLD = 15000;

/**
 * Check whether the name argument matches the fuzzy filter argument
 */
const filterOutlineItem = (name, filter) => {
  if (!filter) {
    return true;
  }

  if (filter.length === 1) {
    // when filter is a single char just check if it starts with the char
    return filter.toLowerCase() === name.toLowerCase()[0];
  }
  return fuzzaldrinScore(name, filter) > FUZZALDRIN_FILTER_THRESHOLD;
};

// Checks if an element is visible inside its parent element
function isVisible(element, parent) {
  const parentRect = parent.getBoundingClientRect();
  const elementRect = element.getBoundingClientRect();

  const parentTop = parentRect.top;
  const parentBottom = parentRect.bottom;
  const elTop = elementRect.top;
  const elBottom = elementRect.bottom;

  return parentTop < elTop && parentBottom > elBottom;
}

export class Outline extends Component {
  constructor(props) {
    super(props);
    this.focusedElRef = null;
    this.state = { filter: "", focusedItem: null, symbols: null };
  }

  static get propTypes() {
    return {
      alphabetizeOutline: PropTypes.bool.isRequired,
      cursorPosition: PropTypes.object,
      flashLineRange: PropTypes.func.isRequired,
      onAlphabetizeClick: PropTypes.func.isRequired,
      selectLocation: PropTypes.func.isRequired,
      selectedLocation: PropTypes.object.isRequired,
      getFunctionSymbols: PropTypes.func.isRequired,
      getClassSymbols: PropTypes.func.isRequired,
      canFetchSymbols: PropTypes.bool,
    };
  }

  componentDidMount() {
    if (!this.props.canFetchSymbols) {
      return;
    }
    this.getClassAndFunctionSymbols();
  }

  componentDidUpdate(prevProps) {
    const { cursorPosition, selectedLocation, canFetchSymbols } = this.props;
    if (cursorPosition && cursorPosition !== prevProps.cursorPosition) {
      this.setFocus(cursorPosition);
    }

    if (
      this.focusedElRef &&
      !isVisible(this.focusedElRef, this.refs.outlineList)
    ) {
      this.focusedElRef.scrollIntoView({ block: "center" });
    }

    // Lets make sure the source text has been loaded and is different
    if (canFetchSymbols && prevProps.selectedLocation !== selectedLocation) {
      this.getClassAndFunctionSymbols();
    }
  }

  async getClassAndFunctionSymbols() {
    const { selectedLocation, getFunctionSymbols, getClassSymbols } =
      this.props;

    const functions = await getFunctionSymbols(selectedLocation);
    const classes = await getClassSymbols(selectedLocation);

    this.setState({ symbols: { functions, classes } });
  }

  async setFocus(cursorPosition) {
    const { symbols } = this.state;

    let classes = [];
    let functions = [];

    if (symbols) {
      ({ classes, functions } = symbols);
    }

    // Find items that enclose the selected location
    const enclosedItems = [...classes, ...functions].filter(
      ({ name, location }) => containsPosition(location, cursorPosition)
    );

    if (!enclosedItems.length) {
      this.setState({ focusedItem: null });
      return;
    }

    // Find the closest item to the selected location to focus
    const closestItem = enclosedItems.reduce((item, closest) =>
      positionAfter(item.location, closest.location) ? item : closest
    );

    this.setState({ focusedItem: closestItem });
  }

  selectItem(selectedItem) {
    const { selectedLocation, selectLocation } = this.props;
    if (!selectedLocation || !selectedItem) {
      return;
    }

    selectLocation(
      createLocation({
        source: selectedLocation.source,
        line: selectedItem.location.start.line,
        column: selectedItem.location.start.column,
      })
    );

    this.setState({ focusedItem: selectedItem });
  }

  onContextMenu(event, func) {
    event.stopPropagation();
    event.preventDefault();

    const { symbols } = this.state;
    this.props.showOutlineContextMenu(event, func, symbols);
  }

  updateFilter = filter => {
    this.setState({ filter: filter.trim() });
  };

  renderPlaceholder() {
    const placeholderMessage = this.props.selectedLocation
      ? L10N.getStr("outline.noFunctions")
      : L10N.getStr("outline.noFileSelected");
    return div(
      {
        className: "outline-pane-info",
      },
      placeholderMessage
    );
  }

  renderLoading() {
    return div(
      {
        className: "outline-pane-info",
      },
      L10N.getStr("loadingText")
    );
  }

  renderFunction(func) {
    const { focusedItem } = this.state;
    const { name, location, parameterNames } = func;
    const isFocused = focusedItem === func;
    return li(
      {
        key: `${name}:${location.start.line}:${location.start.column}`,
        className: classnames("outline-list__element", {
          focused: isFocused,
        }),
        ref: el => {
          if (isFocused) {
            this.focusedElRef = el;
          }
        },
        onClick: () => this.selectItem(func),
        onContextMenu: e => this.onContextMenu(e, func),
      },
      span(
        {
          className: "outline-list__element-icon",
        },
        "λ"
      ),
      React.createElement(PreviewFunction, {
        func: {
          name,
          parameterNames,
        },
      })
    );
  }

  renderClassHeader(klass) {
    return div(
      null,
      span(
        {
          className: "keyword",
        },
        "class"
      ),
      " ",
      klass
    );
  }

  renderClassFunctions(klass, functions) {
    const { symbols } = this.state;

    if (!symbols || klass == null || !functions.length) {
      return null;
    }

    const { focusedItem } = this.state;
    const classFunc = functions.find(func => func.name === klass);
    const classFunctions = functions.filter(func => func.klass === klass);
    const classInfo = symbols.classes.find(c => c.name === klass);

    const item = classFunc || classInfo;
    const isFocused = focusedItem === item;

    return li(
      {
        className: "outline-list__class",
        ref: el => {
          if (isFocused) {
            this.focusedElRef = el;
          }
        },
        key: klass,
      },
      h2(
        {
          className: classnames({
            focused: isFocused,
          }),
          onClick: () => this.selectItem(item),
        },
        classFunc
          ? this.renderFunction(classFunc)
          : this.renderClassHeader(klass)
      ),
      ul(
        {
          className: "outline-list__class-list",
        },
        classFunctions.map(func => this.renderFunction(func))
      )
    );
  }

  renderFunctions(functions) {
    const { filter } = this.state;
    let classes = [...new Set(functions.map(({ klass }) => klass))];
    const namedFunctions = functions.filter(
      ({ name, klass }) =>
        filterOutlineItem(name, filter) && !klass && !classes.includes(name)
    );
    const classFunctions = functions.filter(
      ({ name, klass }) => filterOutlineItem(name, filter) && !!klass
    );

    if (this.props.alphabetizeOutline) {
      const sortByName = (a, b) => (a.name < b.name ? -1 : 1);
      namedFunctions.sort(sortByName);
      classes = classes.sort();
      classFunctions.sort(sortByName);
    }
    return ul(
      {
        ref: "outlineList",
        className: "outline-list devtools-monospace",
        dir: "ltr",
      },
      namedFunctions.map(func => this.renderFunction(func)),
      classes.map(klass => this.renderClassFunctions(klass, classFunctions))
    );
  }

  renderFooter() {
    return div(
      {
        className: "outline-footer",
      },
      button(
        {
          onClick: this.props.onAlphabetizeClick,
          className: this.props.alphabetizeOutline ? "active" : "",
        },
        L10N.getStr("outline.sortLabel")
      )
    );
  }

  render() {
    const { selectedLocation } = this.props;
    const { filter, symbols } = this.state;

    if (!selectedLocation) {
      return this.renderPlaceholder();
    }

    if (!symbols) {
      return this.renderLoading();
    }

    const { functions } = symbols;

    if (functions.length === 0) {
      return this.renderPlaceholder();
    }

    return div(
      {
        className: "outline",
      },
      div(
        null,
        React.createElement(OutlineFilter, {
          filter: filter,
          updateFilter: this.updateFilter,
        }),
        this.renderFunctions(functions),
        this.renderFooter()
      )
    );
  }
}

const mapStateToProps = state => {
  const selectedSourceTextContent = getSelectedSourceTextContent(state);
  return {
    selectedLocation: getSelectedLocation(state),
    canFetchSymbols:
      selectedSourceTextContent && isFulfilled(selectedSourceTextContent),
    cursorPosition: getCursorPosition(state),
  };
};

export default connect(mapStateToProps, {
  selectLocation: actions.selectLocation,
  showOutlineContextMenu: actions.showOutlineContextMenu,
  getFunctionSymbols: actions.getFunctionSymbols,
  getClassSymbols: actions.getClassSymbols,
})(Outline);
