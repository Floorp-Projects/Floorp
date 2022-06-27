/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { showMenu } from "../../context-menu/menu";
import { connect } from "../../utils/connect";
import { score as fuzzaldrinScore } from "fuzzaldrin-plus";
const classnames = require("classnames");

import { containsPosition, positionAfter } from "../../utils/ast";
import { copyToTheClipboard } from "../../utils/clipboard";
import { findFunctionText } from "../../utils/function";

import actions from "../../actions";
import {
  getSelectedSource,
  getSelectedSourceTextContent,
  getSymbols,
  getCursorPosition,
  getContext,
} from "../../selectors";

import OutlineFilter from "./OutlineFilter";
import "./Outline.css";
import PreviewFunction from "../shared/PreviewFunction";

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
    this.state = { filter: "", focusedItem: null };
  }

  static get propTypes() {
    return {
      alphabetizeOutline: PropTypes.bool.isRequired,
      cursorPosition: PropTypes.object,
      cx: PropTypes.object.isRequired,
      flashLineRange: PropTypes.func.isRequired,
      getFunctionText: PropTypes.func.isRequired,
      onAlphabetizeClick: PropTypes.func.isRequired,
      selectLocation: PropTypes.func.isRequired,
      selectedSource: PropTypes.object.isRequired,
      symbols: PropTypes.object.isRequired,
    };
  }

  componentDidUpdate(prevProps) {
    const { cursorPosition, symbols } = this.props;
    if (
      cursorPosition &&
      symbols &&
      cursorPosition !== prevProps.cursorPosition
    ) {
      this.setFocus(cursorPosition);
    }

    if (
      this.focusedElRef &&
      !isVisible(this.focusedElRef, this.refs.outlineList)
    ) {
      this.focusedElRef.scrollIntoView({ block: "center" });
    }
  }

  setFocus(cursorPosition) {
    const { symbols } = this.props;
    let classes = [];
    let functions = [];

    if (symbols) {
      ({ classes, functions } = symbols);
    }

    // Find items that enclose the selected location
    const enclosedItems = [...classes, ...functions].filter(
      ({ name, location }) =>
        name != "anonymous" && containsPosition(location, cursorPosition)
    );

    if (enclosedItems.length == 0) {
      return this.setState({ focusedItem: null });
    }

    // Find the closest item to the selected location to focus
    const closestItem = enclosedItems.reduce((item, closest) =>
      positionAfter(item.location, closest.location) ? item : closest
    );

    this.setState({ focusedItem: closestItem });
  }

  selectItem(selectedItem) {
    const { cx, selectedSource, selectLocation } = this.props;
    if (!selectedSource || !selectedItem) {
      return;
    }

    selectLocation(cx, {
      sourceId: selectedSource.id,
      line: selectedItem.location.start.line,
      column: selectedItem.location.start.column,
    });

    this.setState({ focusedItem: selectedItem });
  }

  onContextMenu(event, func) {
    event.stopPropagation();
    event.preventDefault();

    const { selectedSource, flashLineRange, getFunctionText } = this.props;

    if (!selectedSource) {
      return;
    }

    const sourceLine = func.location.start.line;
    const functionText = getFunctionText(sourceLine);

    const copyFunctionItem = {
      id: "node-menu-copy-function",
      label: L10N.getStr("copyFunction.label"),
      accesskey: L10N.getStr("copyFunction.accesskey"),
      disabled: !functionText,
      click: () => {
        flashLineRange({
          start: sourceLine,
          end: func.location.end.line,
          sourceId: selectedSource.id,
        });
        return copyToTheClipboard(functionText);
      },
    };
    const menuOptions = [copyFunctionItem];
    showMenu(event, menuOptions);
  }

  updateFilter = filter => {
    this.setState({ filter: filter.trim() });
  };

  renderPlaceholder() {
    const placeholderMessage = this.props.selectedSource
      ? L10N.getStr("outline.noFunctions")
      : L10N.getStr("outline.noFileSelected");

    return <div className="outline-pane-info">{placeholderMessage}</div>;
  }

  renderLoading() {
    return (
      <div className="outline-pane-info">{L10N.getStr("loadingText")}</div>
    );
  }

  renderFunction(func) {
    const { focusedItem } = this.state;
    const { name, location, parameterNames } = func;
    const isFocused = focusedItem === func;

    return (
      <li
        key={`${name}:${location.start.line}:${location.start.column}`}
        className={classnames("outline-list__element", { focused: isFocused })}
        ref={el => {
          if (isFocused) {
            this.focusedElRef = el;
          }
        }}
        onClick={() => this.selectItem(func)}
        onContextMenu={e => this.onContextMenu(e, func)}
      >
        <span className="outline-list__element-icon">λ</span>
        <PreviewFunction func={{ name, parameterNames }} />
      </li>
    );
  }

  renderClassHeader(klass) {
    return (
      <div>
        <span className="keyword">class</span> {klass}
      </div>
    );
  }

  renderClassFunctions(klass, functions) {
    const { symbols } = this.props;

    if (!symbols || klass == null || functions.length == 0) {
      return null;
    }

    const { focusedItem } = this.state;
    const classFunc = functions.find(func => func.name === klass);
    const classFunctions = functions.filter(func => func.klass === klass);
    const classInfo = symbols.classes.find(c => c.name === klass);

    const item = classFunc || classInfo;
    const isFocused = focusedItem === item;

    return (
      <li
        className="outline-list__class"
        ref={el => {
          if (isFocused) {
            this.focusedElRef = el;
          }
        }}
        key={klass}
      >
        <h2
          className={classnames("", { focused: isFocused })}
          onClick={() => this.selectItem(item)}
        >
          {classFunc
            ? this.renderFunction(classFunc)
            : this.renderClassHeader(klass)}
        </h2>
        <ul className="outline-list__class-list">
          {classFunctions.map(func => this.renderFunction(func))}
        </ul>
      </li>
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

    return (
      <ul
        ref="outlineList"
        className="outline-list devtools-monospace"
        dir="ltr"
      >
        {namedFunctions.map(func => this.renderFunction(func))}
        {classes.map(klass => this.renderClassFunctions(klass, classFunctions))}
      </ul>
    );
  }

  renderFooter() {
    return (
      <div className="outline-footer">
        <button
          onClick={this.props.onAlphabetizeClick}
          className={this.props.alphabetizeOutline ? "active" : ""}
        >
          {L10N.getStr("outline.sortLabel")}
        </button>
      </div>
    );
  }

  render() {
    const { symbols, selectedSource } = this.props;
    const { filter } = this.state;

    if (!selectedSource) {
      return this.renderPlaceholder();
    }

    if (!symbols) {
      return this.renderLoading();
    }

    const symbolsToDisplay = symbols.functions.filter(
      ({ name }) => name != "anonymous"
    );

    if (symbolsToDisplay.length === 0) {
      return this.renderPlaceholder();
    }

    return (
      <div className="outline">
        <div>
          <OutlineFilter filter={filter} updateFilter={this.updateFilter} />
          {this.renderFunctions(symbolsToDisplay)}
          {this.renderFooter()}
        </div>
      </div>
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const symbols = selectedSource ? getSymbols(state, selectedSource) : null;

  return {
    cx: getContext(state),
    symbols,
    selectedSource,
    cursorPosition: getCursorPosition(state),
    getFunctionText: line => {
      if (selectedSource) {
        const selectedSourceTextContent = getSelectedSourceTextContent(state);
        return findFunctionText(
          line,
          selectedSource,
          selectedSourceTextContent,
          symbols
        );
      }

      return null;
    },
  };
};

export default connect(mapStateToProps, {
  selectLocation: actions.selectLocation,
  flashLineRange: actions.flashLineRange,
})(Outline);
