/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { div, button, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import { createLocation } from "../../utils/location";
import actions from "../../actions";
import {
  getSelectedSource,
  getSelectedLocation,
  getSelectedSourceTextContent,
  getPrettySource,
  getPaneCollapse,
  getGeneratedSource,
  isSourceBlackBoxed,
  canPrettyPrintSource,
  getPrettyPrintMessage,
  isSourceOnSourceMapIgnoreList,
  isSourceMapIgnoreListEnabled,
} from "../../selectors";

import { isPretty, getFilename, shouldBlackbox } from "../../utils/source";

import { PaneToggleButton } from "../shared/Button";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("devtools/client/shared/classnames.js");

import "./Footer.css";

class SourceFooter extends PureComponent {
  constructor() {
    super();

    this.state = { cursorPosition: { line: 0, column: 0 } };
  }

  static get propTypes() {
    return {
      canPrettyPrint: PropTypes.bool.isRequired,
      prettyPrintMessage: PropTypes.string.isRequired,
      endPanelCollapsed: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      jumpToMappedLocation: PropTypes.func.isRequired,
      mappedSource: PropTypes.object,
      selectedSource: PropTypes.object,
      isSelectedSourceBlackBoxed: PropTypes.bool.isRequired,
      sourceLoaded: PropTypes.bool.isRequired,
      toggleBlackBox: PropTypes.func.isRequired,
      togglePaneCollapse: PropTypes.func.isRequired,
      prettyPrintAndSelectSource: PropTypes.func.isRequired,
      isSourceOnIgnoreList: PropTypes.bool.isRequired,
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
      selectedSource,
      canPrettyPrint,
      prettyPrintMessage,
      prettyPrintAndSelectSource,
      sourceLoaded,
    } = this.props;

    if (!selectedSource) {
      return null;
    }

    if (!sourceLoaded && selectedSource.isPrettyPrinted) {
      return div(
        {
          className: "action",
          key: "pretty-loader",
        },
        React.createElement(AccessibleImage, {
          className: "loader spin",
        })
      );
    }

    const type = "prettyPrint";
    return button(
      {
        onClick: () => {
          if (!canPrettyPrint) {
            return;
          }
          prettyPrintAndSelectSource(selectedSource);
        },
        className: classnames("action", type, {
          active: sourceLoaded && canPrettyPrint,
          pretty: isPretty(selectedSource),
        }),
        key: type,
        title: prettyPrintMessage,
        "aria-label": prettyPrintMessage,
        disabled: !canPrettyPrint,
      },
      React.createElement(AccessibleImage, {
        className: type,
      })
    );
  }

  blackBoxButton() {
    const {
      selectedSource,
      isSelectedSourceBlackBoxed,
      toggleBlackBox,
      sourceLoaded,
      isSourceOnIgnoreList,
    } = this.props;

    if (!selectedSource || !shouldBlackbox(selectedSource)) {
      return null;
    }

    let tooltip = isSelectedSourceBlackBoxed
      ? L10N.getStr("sourceFooter.unignore")
      : L10N.getStr("sourceFooter.ignore");

    if (isSourceOnIgnoreList) {
      tooltip = L10N.getStr("sourceFooter.ignoreList");
    }

    const type = "black-box";
    return button(
      {
        onClick: () => toggleBlackBox(selectedSource),
        className: classnames("action", type, {
          active: sourceLoaded,
          blackboxed: isSelectedSourceBlackBoxed || isSourceOnIgnoreList,
        }),
        key: type,
        title: tooltip,
        "aria-label": tooltip,
        disabled: isSourceOnIgnoreList,
      },
      React.createElement(AccessibleImage, {
        className: "blackBox",
      })
    );
  }

  renderToggleButton() {
    if (this.props.horizontal) {
      return null;
    }
    return React.createElement(PaneToggleButton, {
      key: "toggle",
      collapsed: this.props.endPanelCollapsed,
      horizontal: this.props.horizontal,
      handleClick: this.props.togglePaneCollapse,
      position: "end",
    });
  }

  renderCommands() {
    const commands = [this.blackBoxButton(), this.prettyPrintButton()].filter(
      Boolean
    );

    return commands.length
      ? div(
          {
            className: "commands",
          },
          commands
        )
      : null;
  }

  renderSourceSummary() {
    const { mappedSource, jumpToMappedLocation, selectedSource } = this.props;

    if (!mappedSource || !selectedSource || !selectedSource.isOriginal) {
      return null;
    }

    const filename = getFilename(mappedSource);
    const tooltip = L10N.getFormatStr(
      "sourceFooter.mappedSourceTooltip",
      filename
    );
    const title = L10N.getFormatStr("sourceFooter.mappedSource", filename);
    const mappedSourceLocation = createLocation({
      source: selectedSource,
      line: 1,
      column: 1,
    });
    return button(
      {
        className: "mapped-source",
        onClick: () => jumpToMappedLocation(mappedSourceLocation),
        title: tooltip,
      },
      span(null, title)
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
    return div(
      {
        className: "cursor-position",
        title,
      },
      text
    );
  }

  render() {
    return div(
      {
        className: "source-footer",
      },
      div(
        {
          className: "source-footer-start",
        },
        this.renderCommands()
      ),
      div(
        {
          className: "source-footer-end",
        },
        this.renderSourceSummary(),
        this.renderCursorPosition(),
        this.renderToggleButton()
      )
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedLocation = getSelectedLocation(state);
  const sourceTextContent = getSelectedSourceTextContent(state);

  return {
    selectedSource,
    isSelectedSourceBlackBoxed: selectedSource
      ? isSourceBlackBoxed(state, selectedSource)
      : null,
    isSourceOnIgnoreList:
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, selectedSource),
    sourceLoaded: !!sourceTextContent,
    mappedSource: getGeneratedSource(state, selectedSource),
    prettySource: getPrettySource(
      state,
      selectedSource ? selectedSource.id : null
    ),
    endPanelCollapsed: getPaneCollapse(state, "end"),
    canPrettyPrint: selectedLocation
      ? canPrettyPrintSource(state, selectedLocation)
      : false,
    prettyPrintMessage: selectedLocation
      ? getPrettyPrintMessage(state, selectedLocation)
      : null,
  };
};

export default connect(mapStateToProps, {
  prettyPrintAndSelectSource: actions.prettyPrintAndSelectSource,
  toggleBlackBox: actions.toggleBlackBox,
  jumpToMappedLocation: actions.jumpToMappedLocation,
  togglePaneCollapse: actions.togglePaneCollapse,
})(SourceFooter);
