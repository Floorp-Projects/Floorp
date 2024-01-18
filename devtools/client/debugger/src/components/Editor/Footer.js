/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import {
  div,
  button,
  span,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";
import actions from "../../actions/index";
import {
  getSelectedSource,
  getSelectedLocation,
  getSelectedSourceTextContent,
  getPrettySource,
  getPaneCollapse,
  isSourceBlackBoxed,
  canPrettyPrintSource,
  getPrettyPrintMessage,
  isSourceOnSourceMapIgnoreList,
  isSourceMapIgnoreListEnabled,
  getSelectedMappedSource,
} from "../../selectors/index";

import { isPretty, getFilename, shouldBlackbox } from "../../utils/source";

import { PaneToggleButton } from "../shared/Button/index";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");

class SourceFooter extends PureComponent {
  static get propTypes() {
    return {
      canPrettyPrint: PropTypes.bool.isRequired,
      prettyPrintMessage: PropTypes.string.isRequired,
      endPanelCollapsed: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      jumpToMappedLocation: PropTypes.func.isRequired,
      mappedSource: PropTypes.object,
      selectedSource: PropTypes.object,
      selectedLocation: PropTypes.object,
      isSelectedSourceBlackBoxed: PropTypes.bool.isRequired,
      sourceLoaded: PropTypes.bool.isRequired,
      toggleBlackBox: PropTypes.func.isRequired,
      togglePaneCollapse: PropTypes.func.isRequired,
      prettyPrintAndSelectSource: PropTypes.func.isRequired,
      isSourceOnIgnoreList: PropTypes.bool.isRequired,
    };
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
    const { mappedSource, jumpToMappedLocation, selectedLocation } = this.props;

    if (!mappedSource) {
      return null;
    }

    const tooltip = L10N.getFormatStr(
      mappedSource.isOriginal
        ? "sourceFooter.mappedOriginalSource.tooltip"
        : "sourceFooter.mappedGeneratedSource.tooltip",
      mappedSource.url
    );
    const filename = getFilename(mappedSource);
    const label = L10N.getFormatStr(
      mappedSource.isOriginal
        ? "sourceFooter.mappedOriginalSource.title"
        : "sourceFooter.mappedGeneratedSource.title",
      filename
    );
    return button(
      {
        className: "mapped-source",
        onClick: () => jumpToMappedLocation(selectedLocation),
        title: tooltip,
      },
      span(null, label)
    );
  }

  renderCursorPosition() {
    // When we open a new source, there is no particular location selected and the line will be set to zero or falsy
    if (!this.props.selectedLocation || !this.props.selectedLocation.line) {
      return null;
    }

    // Note that line is 1-based while column is 0-based.
    const { line, column } = this.props.selectedLocation;

    const text = L10N.getFormatStr(
      "sourceFooter.currentCursorPosition",
      line,
      column + 1
    );
    const title = L10N.getFormatStr(
      "sourceFooter.currentCursorPosition.tooltip",
      line,
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
    selectedLocation,
    isSelectedSourceBlackBoxed: selectedSource
      ? isSourceBlackBoxed(state, selectedSource)
      : null,
    isSourceOnIgnoreList:
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, selectedSource),
    sourceLoaded: !!sourceTextContent,
    mappedSource: getSelectedMappedSource(state),
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
