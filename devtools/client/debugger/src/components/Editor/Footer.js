/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "devtools/client/shared/vendor/react";
import {
  div,
  button,
  span,
  hr,
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
  getSourceMapErrorForSourceActor,
  areSourceMapsEnabled,
  getShouldSelectOriginalLocation,
  isSourceActorWithSourceMap,
  getSourceMapResolvedURL,
  isSelectedMappedSourceLoading,
} from "../../selectors/index";

import { isPretty, shouldBlackbox } from "../../utils/source";

import { PaneToggleButton } from "../shared/Button/index";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");
const MenuButton = require("resource://devtools/client/shared/components/menu/MenuButton.js");
const MenuItem = require("resource://devtools/client/shared/components/menu/MenuItem.js");
const MenuList = require("resource://devtools/client/shared/components/menu/MenuList.js");

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
    const commands = [
      this.blackBoxButton(),
      this.prettyPrintButton(),
      this.renderSourceMapButton(),
    ].filter(Boolean);

    return commands.length
      ? div(
          {
            className: "commands",
          },
          commands
        )
      : null;
  }

  renderMappedSource() {
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
    const label = L10N.getFormatStr(
      mappedSource.isOriginal
        ? "sourceFooter.mappedOriginalSource.title"
        : "sourceFooter.mappedGeneratedSource.title",
      mappedSource.shortName
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

  getSourceMapLabel() {
    if (!this.props.selectedLocation) {
      return undefined;
    }
    if (!this.props.areSourceMapsEnabled) {
      return L10N.getStr("sourceFooter.sourceMapButton.disabled");
    }
    if (this.props.sourceMapError) {
      return undefined;
    }
    if (!this.props.isSourceActorWithSourceMap) {
      return L10N.getStr("sourceFooter.sourceMapButton.sourceNotMapped");
    }
    if (this.props.selectedLocation.source.isOriginal) {
      return L10N.getStr("sourceFooter.sourceMapButton.isOriginalSource");
    }
    return L10N.getStr("sourceFooter.sourceMapButton.isBundleSource");
  }

  getSourceMapTitle() {
    if (this.props.sourceMapError) {
      return L10N.getFormatStr(
        "sourceFooter.sourceMapButton.errorTitle",
        this.props.sourceMapError
      );
    }
    if (this.props.isSourceMapLoading) {
      return L10N.getStr("sourceFooter.sourceMapButton.loadingTitle");
    }
    return L10N.getStr("sourceFooter.sourceMapButton.title");
  }

  renderSourceMapButton() {
    const { toolboxDoc } = this.context;

    return React.createElement(
      MenuButton,
      {
        menuId: "debugger-source-map-button",
        toolboxDoc,
        className: classnames("devtools-button", "debugger-source-map-button", {
          error: !!this.props.sourceMapError,
          loading: this.props.isSourceMapLoading,
          disabled: !this.props.areSourceMapsEnabled,
          "not-mapped":
            !this.props.selectedLocation?.source.isOriginal &&
            !this.props.isSourceActorWithSourceMap,
          original: this.props.selectedLocation?.source.isOriginal,
        }),
        title: this.getSourceMapTitle(),
        label: this.getSourceMapLabel(),
        icon: true,
      },
      () => this.renderSourceMapMenuItems()
    );
  }

  renderSourceMapMenuItems() {
    const items = [
      React.createElement(MenuItem, {
        className: "menu-item debugger-source-map-enabled",
        checked: this.props.areSourceMapsEnabled,
        label: L10N.getStr("sourceFooter.sourceMapButton.enable"),
        onClick: this.toggleSourceMaps,
      }),
      hr(),
      React.createElement(MenuItem, {
        className: "menu-item debugger-source-map-open-original",
        checked: this.props.shouldSelectOriginalLocation,
        label: L10N.getStr(
          "sourceFooter.sourceMapButton.showOriginalSourceByDefault"
        ),
        onClick: this.toggleSelectOriginalByDefault,
      }),
    ];

    if (this.props.mappedSource) {
      items.push(
        React.createElement(MenuItem, {
          className: "menu-item debugger-jump-mapped-source",
          label: this.props.mappedSource.isOriginal
            ? L10N.getStr("sourceFooter.sourceMapButton.jumpToGeneratedSource")
            : L10N.getStr("sourceFooter.sourceMapButton.jumpToOriginalSource"),
          tooltip: this.props.mappedSource.url,
          onClick: () =>
            this.props.jumpToMappedLocation(this.props.selectedLocation),
        })
      );
    }

    if (this.props.resolvedSourceMapURL) {
      items.push(
        React.createElement(MenuItem, {
          className: "menu-item debugger-source-map-link",
          label: L10N.getStr(
            "sourceFooter.sourceMapButton.openSourceMapInNewTab"
          ),
          onClick: this.openSourceMap,
        })
      );
    }
    return React.createElement(
      MenuList,
      {
        id: "debugger-source-map-list",
      },
      items
    );
  }

  openSourceMap = () => {
    let line, column;
    if (
      this.props.sourceMapError &&
      this.props.sourceMapError.includes("JSON.parse")
    ) {
      const match = this.props.sourceMapError.match(
        /at line (\d+) column (\d+)/
      );
      if (match) {
        line = match[1];
        column = match[2];
      }
    }
    this.props.openSourceMap(
      this.props.resolvedSourceMapURL || this.props.selectedLocation.source.url,
      line,
      column
    );
  };

  toggleSourceMaps = () => {
    this.props.toggleSourceMapsEnabled(!this.props.areSourceMapsEnabled);
  };

  toggleSelectOriginalByDefault = () => {
    this.props.setDefaultSelectedLocation(
      !this.props.shouldSelectOriginalLocation
    );
    this.props.jumpToMappedSelectedLocation();
  };

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
        this.renderMappedSource(),
        this.renderCursorPosition(),
        this.renderToggleButton()
      )
    );
  }
}
SourceFooter.contextTypes = {
  toolboxDoc: PropTypes.object,
};

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedLocation = getSelectedLocation(state);
  const sourceTextContent = getSelectedSourceTextContent(state);

  const areSourceMapsEnabledProp = areSourceMapsEnabled(state);
  const isSourceActorWithSourceMapProp = isSourceActorWithSourceMap(
    state,
    selectedLocation?.sourceActor.id
  );
  const sourceMapError = selectedLocation?.sourceActor
    ? getSourceMapErrorForSourceActor(state, selectedLocation.sourceActor.id)
    : null;
  const mappedSource = getSelectedMappedSource(state);

  const isSourceMapLoading =
    areSourceMapsEnabledProp &&
    isSourceActorWithSourceMapProp &&
    // `mappedSource` will be null while loading, we need another way to know when it is done computing
    !mappedSource &&
    isSelectedMappedSourceLoading(state) &&
    !sourceMapError;

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
    mappedSource,
    isSourceMapLoading,
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

    sourceMapError,
    resolvedSourceMapURL: selectedLocation?.sourceActor
      ? getSourceMapResolvedURL(state, selectedLocation.sourceActor.id)
      : null,
    isSourceActorWithSourceMap: isSourceActorWithSourceMapProp,

    sourceMapURL: selectedLocation?.sourceActor.sourceMapURL,

    areSourceMapsEnabled: areSourceMapsEnabledProp,
    shouldSelectOriginalLocation: getShouldSelectOriginalLocation(state),
  };
};

export default connect(mapStateToProps, {
  prettyPrintAndSelectSource: actions.prettyPrintAndSelectSource,
  toggleBlackBox: actions.toggleBlackBox,
  jumpToMappedLocation: actions.jumpToMappedLocation,
  togglePaneCollapse: actions.togglePaneCollapse,
  toggleSourceMapsEnabled: actions.toggleSourceMapsEnabled,
  setDefaultSelectedLocation: actions.setDefaultSelectedLocation,
  jumpToMappedSelectedLocation: actions.jumpToMappedSelectedLocation,
  openSourceMap: actions.openSourceMap,
})(SourceFooter);
