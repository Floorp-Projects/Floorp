/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { showMenu } from "devtools-contextmenu";
import { connect } from "../../utils/connect";
import { score as fuzzaldrinScore } from "fuzzaldrin-plus";

import { copyToTheClipboard } from "../../utils/clipboard";
import { findFunctionText } from "../../utils/function";

import actions from "../../actions";
import {
  getSelectedSourceWithContent,
  getSymbols,
  getSelectedLocation,
  getContext,
} from "../../selectors";

import OutlineFilter from "./OutlineFilter";
import "./Outline.css";
import PreviewFunction from "../shared/PreviewFunction";
import { uniq, sortBy } from "lodash";

import type {
  AstLocation,
  SymbolDeclarations,
  SymbolDeclaration,
  FunctionDeclaration,
} from "../../workers/parser";
import type { Source, Context } from "../../types";

type Props = {
  cx: Context,
  symbols: SymbolDeclarations,
  selectedSource: ?Source,
  alphabetizeOutline: boolean,
  onAlphabetizeClick: Function,
  selectedLocation: any,
  getFunctionText: Function,
  selectLocation: typeof actions.selectLocation,
  flashLineRange: typeof actions.flashLineRange,
};

type State = {
  filter: string,
};

/**
 * Check whether the name argument matches the fuzzy filter argument
 */
const filterOutlineItem = (name: string, filter: string) => {
  // Set higher to make the fuzzaldrin filter more specific
  const FUZZALDRIN_FILTER_THRESHOLD = 15000;
  if (!filter) {
    return true;
  }

  if (filter.length === 1) {
    // when filter is a single char just check if it starts with the char
    return filter.toLowerCase() === name.toLowerCase()[0];
  }
  return fuzzaldrinScore(name, filter) > FUZZALDRIN_FILTER_THRESHOLD;
};

export class Outline extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    (this: any).updateFilter = this.updateFilter.bind(this);
    this.state = { filter: "" };
  }

  selectItem(location: AstLocation) {
    const { cx, selectedSource, selectLocation } = this.props;
    if (!selectedSource) {
      return;
    }

    selectLocation(cx, {
      sourceId: selectedSource.id,
      line: location.start.line,
      column: location.start.column,
    });
  }

  onContextMenu(event: SyntheticEvent<HTMLElement>, func: SymbolDeclaration) {
    event.stopPropagation();
    event.preventDefault();

    const {
      selectedSource,
      getFunctionText,
      flashLineRange,
      selectedLocation,
    } = this.props;

    const copyFunctionKey = L10N.getStr("copyFunction.accesskey");
    const copyFunctionLabel = L10N.getStr("copyFunction.label");

    if (!selectedSource) {
      return;
    }

    const sourceLine = func.location.start.line;
    const functionText = getFunctionText(sourceLine);

    const copyFunctionItem = {
      id: "node-menu-copy-function",
      label: copyFunctionLabel,
      accesskey: copyFunctionKey,
      disabled: !functionText,
      click: () => {
        flashLineRange({
          start: func.location.start.line,
          end: func.location.end.line,
          sourceId: selectedLocation.sourceId,
        });
        return copyToTheClipboard(functionText);
      },
    };
    const menuOptions = [copyFunctionItem];
    showMenu(event, menuOptions);
  }

  updateFilter(filter: string) {
    this.setState({ filter: filter.trim() });
  }

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

  renderFunction(func: FunctionDeclaration) {
    const { name, location, parameterNames } = func;

    return (
      <li
        key={`${name}:${location.start.line}:${location.start.column}`}
        className="outline-list__element"
        onClick={() => this.selectItem(location)}
        onContextMenu={e => this.onContextMenu(e, func)}
      >
        <span className="outline-list__element-icon">λ</span>
        <PreviewFunction func={{ name, parameterNames }} />
      </li>
    );
  }

  renderClassFunctions(klass: ?string, functions: FunctionDeclaration[]) {
    if (klass == null || functions.length == 0) {
      return null;
    }

    const classFunc = functions.find(func => func.name === klass);
    const classFunctions = functions.filter(func => func.klass === klass);
    const classInfo = this.props.symbols.classes.find(c => c.name === klass);

    const heading = classFunc ? (
      <h2>{this.renderFunction(classFunc)}</h2>
    ) : (
      <h2
        onClick={classInfo ? () => this.selectItem(classInfo.location) : null}
      >
        <span className="keyword">class</span> {klass}
      </h2>
    );

    return (
      <li className="outline-list__class" key={klass}>
        {heading}
        <ul className="outline-list__class-list">
          {classFunctions.map(func => this.renderFunction(func))}
        </ul>
      </li>
    );
  }

  renderFunctions(functions: Array<FunctionDeclaration>) {
    const { filter } = this.state;
    let classes = uniq(functions.map(func => func.klass));
    let namedFunctions = functions.filter(
      func =>
        filterOutlineItem(func.name, filter) &&
        !func.klass &&
        !classes.includes(func.name)
    );

    let classFunctions = functions.filter(
      func => filterOutlineItem(func.name, filter) && !!func.klass
    );

    if (this.props.alphabetizeOutline) {
      namedFunctions = sortBy(namedFunctions, "name");
      classes = sortBy(classes, "klass");
      classFunctions = sortBy(classFunctions, "name");
    }

    return (
      <ul className="outline-list devtools-monospace">
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

    if (!symbols || symbols.loading) {
      return this.renderLoading();
    }

    const symbolsToDisplay = symbols.functions.filter(
      func => func.name != "anonymous"
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
  const selectedSource = getSelectedSourceWithContent(state);
  const symbols = selectedSource ? getSymbols(state, selectedSource) : null;

  return {
    cx: getContext(state),
    symbols,
    selectedSource: (selectedSource: ?Source),
    selectedLocation: getSelectedLocation(state),
    getFunctionText: line => {
      if (selectedSource) {
        return findFunctionText(line, selectedSource, symbols);
      }

      return null;
    },
  };
};

export default connect(
  mapStateToProps,
  {
    selectLocation: actions.selectLocation,
    flashLineRange: actions.flashLineRange,
  }
)(Outline);
