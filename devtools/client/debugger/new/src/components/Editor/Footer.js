/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "react-redux";
import classnames from "classnames";
import Svg from "../shared/Svg";
import actions from "../../actions";
import {
  getSelectedSource,
  getPrettySource,
  getPaneCollapse
} from "../../selectors";

import {
  isPretty,
  isLoaded,
  getFilename,
  isOriginal,
  isLoading,
  shouldBlackbox
} from "../../utils/source";
import { getGeneratedSource } from "../../reducers/sources";
import { shouldShowFooter, shouldShowPrettyPrint } from "../../utils/editor";

import { PaneToggleButton } from "../shared/Button";

import type { Source } from "../../types";

import "./Footer.css";

type CursorPosition = {
  line: number,
  column: number
};

type Props = {
  selectedSource: Source,
  mappedSource: Source,
  endPanelCollapsed: boolean,
  editor: Object,
  horizontal: boolean,
  togglePrettyPrint: string => void,
  toggleBlackBox: Object => void,
  jumpToMappedLocation: (Source: any) => void,
  togglePaneCollapse: () => void
};

type State = {
  cursorPosition: CursorPosition
};

class SourceFooter extends PureComponent<Props, State> {
  constructor() {
    super();

    this.state = { cursorPosition: { line: 1, column: 1 } };
  }

  componentDidMount() {
    const { editor } = this.props;
    editor.codeMirror.on("cursorActivity", this.onCursorChange);
  }

  componentWillUnmount() {
    const { editor } = this.props;
    editor.codeMirror.off("cursorActivity", this.onCursorChange);
  }

  prettyPrintButton() {
    const { selectedSource, togglePrettyPrint } = this.props;

    if (isLoading(selectedSource) && selectedSource.isPrettyPrinted) {
      return (
        <div className="loader" key="pretty-loader">
          <Svg name="loader" />
        </div>
      );
    }

    if (!shouldShowPrettyPrint(selectedSource)) {
      return;
    }

    const tooltip = L10N.getStr("sourceTabs.prettyPrint");
    const sourceLoaded = selectedSource && isLoaded(selectedSource);

    const type = "prettyPrint";
    return (
      <button
        onClick={() => togglePrettyPrint(selectedSource.id)}
        className={classnames("action", type, {
          active: sourceLoaded,
          pretty: isPretty(selectedSource)
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <img className={type} />
      </button>
    );
  }

  blackBoxButton() {
    const { selectedSource, toggleBlackBox } = this.props;
    const sourceLoaded = selectedSource && isLoaded(selectedSource);

    if (!shouldBlackbox(selectedSource)) {
      return;
    }

    const blackboxed = selectedSource.isBlackBoxed;

    const tooltip = L10N.getStr("sourceFooter.blackbox");
    const type = "black-box";

    return (
      <button
        onClick={() => toggleBlackBox(selectedSource)}
        className={classnames("action", type, {
          active: sourceLoaded,
          blackboxed: blackboxed
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <img className="blackBox" />
      </button>
    );
  }

  blackBoxSummary() {
    const { selectedSource } = this.props;

    if (!selectedSource || !selectedSource.isBlackBoxed) {
      return;
    }

    return (
      <span className="blackbox-summary" key="blackbox-summary">
        {L10N.getStr("sourceFooter.blackboxed")}
      </span>
    );
  }

  renderToggleButton() {
    if (this.props.horizontal) {
      return;
    }

    return (
      <PaneToggleButton
        position="end"
        key="toggle"
        collapsed={!this.props.endPanelCollapsed}
        horizontal={this.props.horizontal}
        handleClick={this.props.togglePaneCollapse}
      />
    );
  }

  renderCommands() {
    const commands = [
      this.prettyPrintButton(),
      this.blackBoxButton(),
      this.blackBoxSummary()
    ].filter(Boolean);

    return commands.length ? <div className="commands">{commands}</div> : null;
  }

  renderSourceSummary() {
    const { mappedSource, jumpToMappedLocation, selectedSource } = this.props;

    if (!mappedSource || !isOriginal(selectedSource)) {
      return null;
    }

    const filename = getFilename(mappedSource);
    const tooltip = L10N.getFormatStr(
      "sourceFooter.mappedSourceTooltip",
      filename
    );
    const title = L10N.getFormatStr("sourceFooter.mappedSource", filename);
    const mappedSourceLocation = {
      sourceId: selectedSource.id,
      line: 1,
      column: 1
    };
    return (
      <button
        className="mapped-source"
        onClick={() => jumpToMappedLocation(mappedSourceLocation)}
        title={tooltip}
      >
        <span>{title}</span>
      </button>
    );
  }

  onCursorChange = event => {
    const { line, ch } = event.doc.getCursor();
    this.setState({ cursorPosition: { line, column: ch } });
  };

  renderCursorPosition() {
    const { cursorPosition } = this.state;

    const text = L10N.getFormatStr(
      "sourceFooter.currentCursorPosition",
      cursorPosition.line + 1,
      cursorPosition.column + 1
    );
    return <span className="cursor-position">{text}</span>;
  }

  render() {
    const { selectedSource, horizontal } = this.props;

    if (!shouldShowFooter(selectedSource, horizontal)) {
      return null;
    }

    return (
      <div className="source-footer">
        {this.renderCommands()}
        {this.renderSourceSummary()}
        {this.renderToggleButton()}
        {this.renderCursorPosition()}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedId = selectedSource.id;

  return {
    selectedSource,
    mappedSource: getGeneratedSource(state, selectedSource),
    prettySource: getPrettySource(state, selectedId),
    endPanelCollapsed: getPaneCollapse(state, "end")
  };
};

export default connect(
  mapStateToProps,
  {
    togglePrettyPrint: actions.togglePrettyPrint,
    toggleBlackBox: actions.toggleBlackBox,
    jumpToMappedLocation: actions.jumpToMappedLocation,
    togglePaneCollapse: actions.togglePaneCollapse
  }
)(SourceFooter);
