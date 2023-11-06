/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { div, button, span } from "react-dom-factories";
import PropTypes from "prop-types";
import AccessibleImage from "../shared/AccessibleImage";
import { showMenu } from "../../context-menu/menu";
import { connect } from "../../utils/connect";
import actions from "../../actions";

import {
  getSelectedFrame,
  getCurrentThread,
  getSelectedSource,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getPauseReason,
  isMapScopesEnabled,
  getLastExpandedScopes,
  getIsCurrentThreadPaused,
} from "../../selectors";
import {
  getScopesItemsForSelectedFrame,
  getScopeItemPath,
} from "../../utils/pause/scopes";
import { clientCommands } from "../../client/firefox";

import { objectInspector } from "devtools/client/shared/components/reps/index";

import "./Scopes.css";

const { ObjectInspector } = objectInspector;

class Scopes extends PureComponent {
  constructor(props) {
    const { why, selectedFrame, originalFrameScopes, generatedFrameScopes } =
      props;

    super(props);

    this.state = {
      originalScopes: getScopesItemsForSelectedFrame(
        why,
        selectedFrame,
        originalFrameScopes
      ),
      generatedScopes: getScopesItemsForSelectedFrame(
        why,
        selectedFrame,
        generatedFrameScopes
      ),
    };
  }

  static get propTypes() {
    return {
      addWatchpoint: PropTypes.func.isRequired,
      expandedScopes: PropTypes.array.isRequired,
      generatedFrameScopes: PropTypes.object,
      highlightDomElement: PropTypes.func.isRequired,
      isLoading: PropTypes.bool.isRequired,
      isPaused: PropTypes.bool.isRequired,
      mapScopesEnabled: PropTypes.bool.isRequired,
      openElementInInspector: PropTypes.func.isRequired,
      openLink: PropTypes.func.isRequired,
      originalFrameScopes: PropTypes.object,
      removeWatchpoint: PropTypes.func.isRequired,
      setExpandedScope: PropTypes.func.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
      why: PropTypes.object.isRequired,
      selectedFrame: PropTypes.object,
      toggleMapScopes: PropTypes.func.isRequired,
      selectedSource: PropTypes.object.isRequired,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const {
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
      isPaused,
      selectedSource,
    } = this.props;
    const isPausedChanged = isPaused !== nextProps.isPaused;
    const selectedFrameChanged = selectedFrame !== nextProps.selectedFrame;
    const originalFrameScopesChanged =
      originalFrameScopes !== nextProps.originalFrameScopes;
    const generatedFrameScopesChanged =
      generatedFrameScopes !== nextProps.generatedFrameScopes;
    const selectedSourceChanged = selectedSource !== nextProps.selectedSource;

    if (
      isPausedChanged ||
      selectedFrameChanged ||
      originalFrameScopesChanged ||
      generatedFrameScopesChanged ||
      selectedSourceChanged
    ) {
      this.setState({
        originalScopes: getScopesItemsForSelectedFrame(
          nextProps.why,
          nextProps.selectedFrame,
          nextProps.originalFrameScopes
        ),
        generatedScopes: getScopesItemsForSelectedFrame(
          nextProps.why,
          nextProps.selectedFrame,
          nextProps.generatedFrameScopes
        ),
      });
    }
  }

  onContextMenu = (event, item) => {
    const { addWatchpoint, removeWatchpoint } = this.props;

    if (!item.parent || !item.contents.configurable) {
      return;
    }

    if (!item.contents || item.contents.watchpoint) {
      const removeWatchpointLabel = L10N.getStr("watchpoints.removeWatchpoint");

      const removeWatchpointItem = {
        id: "node-menu-remove-watchpoint",
        label: removeWatchpointLabel,
        disabled: false,
        click: () => removeWatchpoint(item),
      };

      const menuItems = [removeWatchpointItem];
      showMenu(event, menuItems);
      return;
    }

    const addSetWatchpointLabel = L10N.getStr("watchpoints.setWatchpoint");
    const addGetWatchpointLabel = L10N.getStr("watchpoints.getWatchpoint");
    const addGetOrSetWatchpointLabel = L10N.getStr(
      "watchpoints.getOrSetWatchpoint"
    );
    const watchpointsSubmenuLabel = L10N.getStr("watchpoints.submenu");

    const addSetWatchpointItem = {
      id: "node-menu-add-set-watchpoint",
      label: addSetWatchpointLabel,
      disabled: false,
      click: () => addWatchpoint(item, "set"),
    };

    const addGetWatchpointItem = {
      id: "node-menu-add-get-watchpoint",
      label: addGetWatchpointLabel,
      disabled: false,
      click: () => addWatchpoint(item, "get"),
    };

    const addGetOrSetWatchpointItem = {
      id: "node-menu-add-get-watchpoint",
      label: addGetOrSetWatchpointLabel,
      disabled: false,
      click: () => addWatchpoint(item, "getorset"),
    };

    const watchpointsSubmenuItem = {
      id: "node-menu-watchpoints",
      label: watchpointsSubmenuLabel,
      disabled: false,
      click: () => addWatchpoint(item, "set"),
      submenu: [
        addSetWatchpointItem,
        addGetWatchpointItem,
        addGetOrSetWatchpointItem,
      ],
    };

    const menuItems = [watchpointsSubmenuItem];
    showMenu(event, menuItems);
  };

  renderWatchpointButton = item => {
    const { removeWatchpoint } = this.props;

    if (
      !item ||
      !item.contents ||
      !item.contents.watchpoint ||
      typeof L10N === "undefined"
    ) {
      return null;
    }

    const { watchpoint } = item.contents;
    return button({
      className: `remove-watchpoint-${watchpoint}`,
      title: L10N.getStr("watchpoints.removeWatchpointTooltip"),
      onClick: e => {
        e.stopPropagation();
        removeWatchpoint(item);
      },
    });
  };

  renderScopesList() {
    const {
      isLoading,
      openLink,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
      mapScopesEnabled,
      selectedFrame,
      setExpandedScope,
      expandedScopes,
      selectedSource,
    } = this.props;

    if (!selectedSource) {
      return div(
        { className: "pane scopes-list" },
        div({ className: "pane-info" }, L10N.getStr("scopes.notAvailable"))
      );
    }

    const { originalScopes, generatedScopes } = this.state;
    let scopes = null;

    if (
      selectedSource.isOriginal &&
      !selectedSource.isPrettyPrinted &&
      !selectedFrame.generatedLocation?.source.isWasm
    ) {
      if (!mapScopesEnabled) {
        return div(
          { className: "pane scopes-list" },
          div(
            {
              className: "pane-info no-original-scopes-info",
              "aria-role": "status",
            },
            span(
              { className: "info icon" },
              React.createElement(AccessibleImage, { className: "sourcemap" })
            ),
            L10N.getFormatStr(
              "scopes.noOriginalScopes",
              L10N.getStr("scopes.showOriginalScopes")
            )
          )
        );
      }
      if (isLoading) {
        return div(
          {
            className: "pane scopes-list",
          },
          div(
            { className: "pane-info" },
            span(
              { className: "info icon" },
              React.createElement(AccessibleImage, { className: "loader" })
            ),
            L10N.getStr("scopes.loadingOriginalScopes")
          )
        );
      }
      scopes = originalScopes;
    } else {
      if (isLoading) {
        return div(
          {
            className: "pane scopes-list",
          },
          div(
            { className: "pane-info" },
            span(
              { className: "info icon" },
              React.createElement(AccessibleImage, { className: "loader" })
            ),
            L10N.getStr("loadingText")
          )
        );
      }
      scopes = generatedScopes;
    }

    function initiallyExpanded(item) {
      return expandedScopes.some(path => path == getScopeItemPath(item));
    }

    if (scopes && !!scopes.length) {
      return div(
        {
          className: "pane scopes-list",
        },
        React.createElement(ObjectInspector, {
          roots: scopes,
          autoExpandAll: false,
          autoExpandDepth: 1,
          client: clientCommands,
          createElement: tagName => document.createElement(tagName),
          disableWrap: true,
          dimTopLevelWindow: true,
          frame: selectedFrame,
          mayUseCustomFormatter: true,
          openLink: openLink,
          onDOMNodeClick: grip => openElementInInspector(grip),
          onInspectIconClick: grip => openElementInInspector(grip),
          onDOMNodeMouseOver: grip => highlightDomElement(grip),
          onDOMNodeMouseOut: grip => unHighlightDomElement(grip),
          onContextMenu: this.onContextMenu,
          setExpanded: (path, expand) =>
            setExpandedScope(selectedFrame, path, expand),
          initiallyExpanded: initiallyExpanded,
          renderItemActions: this.renderWatchpointButton,
          shouldRenderTooltip: true,
        })
      );
    }

    return div(
      {
        className: "pane scopes-list",
      },
      div(
        {
          className: "pane-info",
        },
        L10N.getStr("scopes.notAvailable")
      )
    );
  }

  render() {
    return div(
      {
        className: "scopes-content",
      },
      this.renderScopesList()
    );
  }
}

const mapStateToProps = state => {
  // This component doesn't need any prop when we are not paused
  const selectedFrame = getSelectedFrame(state, getCurrentThread(state));
  const why = getPauseReason(state, selectedFrame.thread);
  const expandedScopes = getLastExpandedScopes(state, selectedFrame.thread);
  const isPaused = getIsCurrentThreadPaused(state);
  const selectedSource = getSelectedSource(state);

  if (!selectedFrame) {
    return {};
  }

  let originalFrameScopes;
  let generatedFrameScopes;
  let isLoading;
  let mapScopesEnabled;

  if (
    selectedSource?.isOriginal &&
    !selectedSource?.isPrettyPrinted &&
    !selectedFrame.generatedLocation?.source.isWasm
  ) {
    const { scope, pending: originalPending } = getOriginalFrameScope(
      state,
      selectedFrame
    ) || {
      scope: null,
      pending: false,
    };
    originalFrameScopes = scope;
    isLoading = originalPending;
    mapScopesEnabled = isMapScopesEnabled(state);
  } else {
    const { scope, pending: generatedPending } = getGeneratedFrameScope(
      state,
      selectedFrame
    ) || {
      scope: null,
      pending: false,
    };
    generatedFrameScopes = scope;
    isLoading = generatedPending;
  }

  return {
    originalFrameScopes,
    generatedFrameScopes,
    mapScopesEnabled,
    selectedFrame,
    isLoading,
    why,
    expandedScopes,
    isPaused,
    selectedSource,
  };
};

export default connect(mapStateToProps, {
  openLink: actions.openLink,
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
  setExpandedScope: actions.setExpandedScope,
  addWatchpoint: actions.addWatchpoint,
  removeWatchpoint: actions.removeWatchpoint,
  toggleMapScopes: actions.toggleMapScopes,
})(Scopes);
