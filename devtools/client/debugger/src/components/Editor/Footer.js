/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import classnames from "classnames";
import actions from "../../actions";
import {
  getSelectedSource,
  getSelectedSourceTextContent,
  getPrettySource,
  getPaneCollapse,
  getContext,
  getGeneratedSource,
  isSourceBlackBoxed,
  canPrettyPrintSource,
} from "../../selectors";

import { isPretty, getFilename, shouldBlackbox } from "../../utils/source";

import { PaneToggleButton } from "../shared/Button";
import AccessibleImage from "../shared/AccessibleImage";

import "./Footer.css";

class SourceFooter extends PureComponent {
  constructor() {
    super();

    this.state = { cursorPosition: { line: 0, column: 0 } };
  }

  static get propTypes() {
    return {
      canPrettyPrint: PropTypes.bool.isRequired,
      cx: PropTypes.object.isRequired,
      endPanelCollapsed: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      jumpToMappedLocation: PropTypes.func.isRequired,
      mappedSource: PropTypes.object,
      selectedSource: PropTypes.object,
      isSelectedSourceBlackBoxed: PropTypes.bool.isRequired,
      sourceLoaded: PropTypes.bool.isRequired,
      toggleBlackBox: PropTypes.func.isRequired,
      togglePaneCollapse: PropTypes.func.isRequired,
      togglePrettyPrint: PropTypes.func.isRequired,
    };
  }

  componentDidUpdate() {
    const eventDoc = document.querySelector(".editor-mount .CodeMirror");
    // querySelector can return null
    if (eventDoc) {
      this.toggleCodeMirror(eventDoc, true);
    }
  }

  componentWillUnmount() {
    const eventDoc = document.querySelector(".editor-mount .CodeMirror");

    if (eventDoc) {
      this.toggleCodeMirror(eventDoc, false);
    }
  }

  toggleCodeMirror(eventDoc, toggle) {
    if (toggle === true) {
      eventDoc.CodeMirror.on("cursorActivity", this.onCursorChange);
    } else {
      eventDoc.CodeMirror.off("cursorActivity", this.onCursorChange);
    }
  }

  prettyPrintButton() {
    const {
      cx,
      selectedSource,
      canPrettyPrint,
      togglePrettyPrint,
      sourceLoaded,
    } = this.props;

    if (!selectedSource) {
      return null;
    }

    if (!sourceLoaded && selectedSource.isPrettyPrinted) {
      return (
        <div className="action" key="pretty-loader">
          <AccessibleImage className="loader spin" />
        </div>
      );
    }

    if (!canPrettyPrint) {
      return null;
    }

    const tooltip = L10N.getStr("sourceTabs.prettyPrint");

    const type = "prettyPrint";
    return (
      <button
        onClick={() => togglePrettyPrint(cx, selectedSource.id)}
        className={classnames("action", type, {
          active: sourceLoaded,
          pretty: isPretty(selectedSource),
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <AccessibleImage className={type} />
      </button>
    );
  }

  blackBoxButton() {
    const {
      cx,
      selectedSource,
      isSelectedSourceBlackBoxed,
      toggleBlackBox,
      sourceLoaded,
    } = this.props;

    if (!selectedSource || !shouldBlackbox(selectedSource)) {
      return null;
    }

    const blackboxed = isSelectedSourceBlackBoxed;

    const tooltip = blackboxed
      ? L10N.getStr("sourceFooter.unignore")
      : L10N.getStr("sourceFooter.ignore");

    const type = "black-box";

    return (
      <button
        onClick={() => toggleBlackBox(cx, selectedSource)}
        className={classnames("action", type, {
          active: sourceLoaded,
          blackboxed,
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <AccessibleImage className="blackBox" />
      </button>
    );
  }

  renderToggleButton() {
    if (this.props.horizontal) {
      return null;
    }

    return (
      <PaneToggleButton
        key="toggle"
        collapsed={this.props.endPanelCollapsed}
        horizontal={this.props.horizontal}
        handleClick={this.props.togglePaneCollapse}
        position="end"
      />
    );
  }

  renderCommands() {
    const commands = [this.blackBoxButton(), this.prettyPrintButton()].filter(
      Boolean
    );

    return commands.length ? <div className="commands">{commands}</div> : null;
  }

  renderSourceSummary() {
    const {
      cx,
      mappedSource,
      jumpToMappedLocation,
      selectedSource,
    } = this.props;

    if (!mappedSource || !selectedSource || !selectedSource.isOriginal) {
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
      column: 1,
    };
    return (
      <button
        className="mapped-source"
        onClick={() => jumpToMappedLocation(cx, mappedSourceLocation)}
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
    if (!this.props.selectedSource) {
      return null;
    }

    const { line, column } = this.state.cursorPosition;

    const text = L10N.getFormatStr(
      "sourceFooter.currentCursorPosition",
      line + 1,
      column + 1
    );
    const title = L10N.getFormatStr(
      "sourceFooter.currentCursorPosition.tooltip",
      line + 1,
      column + 1
    );
    return (
      <div className="cursor-position" title={title}>
        {text}
      </div>
    );
  }

  render() {
    return (
      <div className="source-footer">
        <div className="source-footer-start">{this.renderCommands()}</div>
        <div className="source-footer-end">
          {this.renderSourceSummary()}
          {this.renderCursorPosition()}
          {this.renderToggleButton()}
        </div>
      </div>
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const sourceTextContent = getSelectedSourceTextContent(state);

  return {
    cx: getContext(state),
    selectedSource,
    isSelectedSourceBlackBoxed: selectedSource
      ? isSourceBlackBoxed(state, selectedSource)
      : null,
    sourceLoaded: !!sourceTextContent,
    mappedSource: getGeneratedSource(state, selectedSource),
    prettySource: getPrettySource(
      state,
      selectedSource ? selectedSource.id : null
    ),
    endPanelCollapsed: getPaneCollapse(state, "end"),
    canPrettyPrint: selectedSource
      ? canPrettyPrintSource(state, selectedSource.id)
      : false,
  };
};

export default connect(mapStateToProps, {
  togglePrettyPrint: actions.togglePrettyPrint,
  toggleBlackBox: actions.toggleBlackBox,
  jumpToMappedLocation: actions.jumpToMappedLocation,
  togglePaneCollapse: actions.togglePaneCollapse,
})(SourceFooter);
