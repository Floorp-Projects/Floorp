/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { showMenu } from "../../context-menu/menu";
import { connect } from "../../utils/connect";
import actions from "../../actions";

import {
  getSelectedSource,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getPauseReason,
  isMapScopesEnabled,
  getThreadContext,
  getLastExpandedScopes,
  getIsCurrentThreadPaused,
} from "../../selectors";
import { getScopes } from "../../utils/pause/scopes";
import { getScopeItemPath } from "../../utils/pause/scopes/utils";

import { objectInspector } from "devtools/client/shared/components/reps/index";

import "./Scopes.css";

const { ObjectInspector } = objectInspector;

class Scopes extends PureComponent {
  constructor(props) {
    const {
      why,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
    } = props;

    super(props);

    this.state = {
      originalScopes: getScopes(why, selectedFrame, originalFrameScopes),
      generatedScopes: getScopes(why, selectedFrame, generatedFrameScopes),
      showOriginal: true,
    };
  }

  componentWillReceiveProps(nextProps) {
    const {
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
      isPaused,
    } = this.props;
    const isPausedChanged = isPaused !== nextProps.isPaused;
    const selectedFrameChanged = selectedFrame !== nextProps.selectedFrame;
    const originalFrameScopesChanged =
      originalFrameScopes !== nextProps.originalFrameScopes;
    const generatedFrameScopesChanged =
      generatedFrameScopes !== nextProps.generatedFrameScopes;

    if (
      isPausedChanged ||
      selectedFrameChanged ||
      originalFrameScopesChanged ||
      generatedFrameScopesChanged
    ) {
      this.setState({
        originalScopes: getScopes(
          nextProps.why,
          nextProps.selectedFrame,
          nextProps.originalFrameScopes
        ),
        generatedScopes: getScopes(
          nextProps.why,
          nextProps.selectedFrame,
          nextProps.generatedFrameScopes
        ),
      });
    }
  }

  onToggleMapScopes = () => {
    this.props.toggleMapScopes();
  };

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
      return showMenu(event, menuItems);
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
    return (
      <button
        className={`remove-${watchpoint}-watchpoint`}
        title={L10N.getStr("watchpoints.removeWatchpointTooltip")}
        onClick={e => {
          e.stopPropagation();
          removeWatchpoint(item);
        }}
      />
    );
  };

  renderScopesList() {
    const {
      cx,
      isLoading,
      openLink,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
      mapScopesEnabled,
      setExpandedScope,
      expandedScopes,
    } = this.props;
    const { originalScopes, generatedScopes, showOriginal } = this.state;

    const scopes =
      (showOriginal && mapScopesEnabled && originalScopes) || generatedScopes;

    function initiallyExpanded(item) {
      return expandedScopes.some(path => path == getScopeItemPath(item));
    }

    if (scopes && scopes.length > 0 && !isLoading) {
      return (
        <div className="pane scopes-list">
          <ObjectInspector
            roots={scopes}
            autoExpandAll={false}
            autoExpandDepth={1}
            disableWrap={true}
            dimTopLevelWindow={true}
            openLink={openLink}
            onDOMNodeClick={grip => openElementInInspector(grip)}
            onInspectIconClick={grip => openElementInInspector(grip)}
            onDOMNodeMouseOver={grip => highlightDomElement(grip)}
            onDOMNodeMouseOut={grip => unHighlightDomElement(grip)}
            onContextMenu={this.onContextMenu}
            setExpanded={(path, expand) => setExpandedScope(cx, path, expand)}
            initiallyExpanded={initiallyExpanded}
            renderItemActions={this.renderWatchpointButton}
            shouldRenderTooltip={true}
          />
        </div>
      );
    }

    let stateText = L10N.getStr("scopes.notPaused");
    if (this.props.isPaused) {
      if (isLoading) {
        stateText = L10N.getStr("loadingText");
      } else {
        stateText = L10N.getStr("scopes.notAvailable");
      }
    }

    return (
      <div className="pane scopes-list">
        <div className="pane-info">{stateText}</div>
      </div>
    );
  }

  render() {
    return <div className="scopes-content">{this.renderScopesList()}</div>;
  }
}

const mapStateToProps = state => {
  const cx = getThreadContext(state);
  const selectedFrame = getSelectedFrame(state, cx.thread);
  const selectedSource = getSelectedSource(state);

  const {
    scope: originalFrameScopes,
    pending: originalPending,
  } = getOriginalFrameScope(
    state,
    cx.thread,
    selectedSource?.id,
    selectedFrame?.id
  ) || { scope: null, pending: false };

  const {
    scope: generatedFrameScopes,
    pending: generatedPending,
  } = getGeneratedFrameScope(state, cx.thread, selectedFrame?.id) || {
    scope: null,
    pending: false,
  };

  return {
    cx,
    selectedFrame,
    mapScopesEnabled: isMapScopesEnabled(state),
    isLoading: generatedPending || originalPending,
    why: getPauseReason(state, cx.thread),
    originalFrameScopes,
    generatedFrameScopes,
    expandedScopes: getLastExpandedScopes(state, cx.thread),
    isPaused: getIsCurrentThreadPaused(state),
  };
};

export default connect(mapStateToProps, {
  openLink: actions.openLink,
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
  toggleMapScopes: actions.toggleMapScopes,
  setExpandedScope: actions.setExpandedScope,
  addWatchpoint: actions.addWatchpoint,
  removeWatchpoint: actions.removeWatchpoint,
})(Scopes);
