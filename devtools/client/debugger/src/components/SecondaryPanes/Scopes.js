/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { showMenu } from "devtools-contextmenu";
import { connect } from "../../utils/connect";
import actions from "../../actions";
import { createObjectClient } from "../../client/firefox";
import { features } from "../../utils/prefs";

import {
  getSelectedSource,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getPauseReason,
  isMapScopesEnabled,
  getThreadContext,
  getLastExpandedScopes,
} from "../../selectors";
import { getScopes } from "../../utils/pause/scopes";
import { getScopeItemPath } from "../../utils/pause/scopes/utils";

// eslint-disable-next-line import/named
import { objectInspector } from "devtools-reps";

import type { ThreadContext, Why } from "../../types";
import type { NamedValue } from "../../utils/pause/scopes/types";

import "./Scopes.css";

const { ObjectInspector } = objectInspector;

type Props = {
  cx: ThreadContext,
  selectedFrame: Object,
  generatedFrameScopes: Object,
  originalFrameScopes: Object | null,
  isLoading: boolean,
  why: Why,
  mapScopesEnabled: boolean,
  openLink: typeof actions.openLink,
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  highlightDomElement: typeof actions.highlightDomElement,
  unHighlightDomElement: typeof actions.unHighlightDomElement,
  toggleMapScopes: typeof actions.toggleMapScopes,
  setExpandedScope: typeof actions.setExpandedScope,
  addWatchpoint: typeof actions.addWatchpoint,
  removeWatchpoint: typeof actions.removeWatchpoint,
  expandedScopes: string[],
};

type State = {
  originalScopes: ?(NamedValue[]),
  generatedScopes: ?(NamedValue[]),
  showOriginal: boolean,
};

class Scopes extends PureComponent<Props, State> {
  constructor(props: Props, ...args) {
    const {
      why,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
    } = props;

    super(props, ...args);

    this.state = {
      originalScopes: getScopes(why, selectedFrame, originalFrameScopes),
      generatedScopes: getScopes(why, selectedFrame, generatedFrameScopes),
      showOriginal: true,
    };
  }

  componentWillReceiveProps(nextProps) {
    const {
      cx,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
    } = this.props;
    const isPausedChanged = cx.isPaused !== nextProps.cx.isPaused;
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

    if (!features.watchpoints || !item.parent || !item.parent.contents) {
      return;
    }

    if (!item.contents || item.contents.watchpoint) {
      const removeWatchpointItem = {
        id: "node-menu-remove-watchpoint",
        // NOTE: we're going to update the UI to add a "break on..."
        // sub menu. At that point we'll translate the strings. bug 1580591
        label: "Remove watchpoint",
        disabled: false,
        click: () => removeWatchpoint(item),
      };

      const menuItems = [removeWatchpointItem];
      return showMenu(event, menuItems);
    }

    // NOTE: we're going to update the UI to add a "break on..."
    // sub menu. At that point we'll translate the strings. bug 1580591
    const addSetWatchpointLabel = "Pause on set";
    const addGetWatchpointLabel = "Pause on get";

    const addSetWatchpoint = {
      id: "node-menu-add-set-watchpoint",
      label: addSetWatchpointLabel,
      disabled: false,
      click: () => addWatchpoint(item, "set"),
    };

    const addGetWatchpoint = {
      id: "node-menu-add-get-watchpoint",
      label: addGetWatchpointLabel,
      disabled: false,
      click: () => addWatchpoint(item, "get"),
    };

    const menuItems = [addGetWatchpoint, addSetWatchpoint];
    showMenu(event, menuItems);
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
            createObjectClient={grip => createObjectClient(grip)}
            onDOMNodeClick={grip => openElementInInspector(grip)}
            onInspectIconClick={grip => openElementInInspector(grip)}
            onDOMNodeMouseOver={grip => highlightDomElement(grip)}
            onDOMNodeMouseOut={grip => unHighlightDomElement(grip)}
            onContextMenu={this.onContextMenu}
            setExpanded={(path, expand) => setExpandedScope(cx, path, expand)}
            initiallyExpanded={initiallyExpanded}
          />
        </div>
      );
    }

    let stateText = L10N.getStr("scopes.notPaused");
    if (cx.isPaused) {
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
    selectedSource && selectedSource.id,
    selectedFrame && selectedFrame.id
  ) || { scope: null, pending: false };

  const {
    scope: generatedFrameScopes,
    pending: generatedPending,
  } = getGeneratedFrameScope(
    state,
    cx.thread,
    selectedFrame && selectedFrame.id
  ) || {
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
  };
};

export default connect(
  mapStateToProps,
  {
    openLink: actions.openLink,
    openElementInInspector: actions.openElementInInspectorCommand,
    highlightDomElement: actions.highlightDomElement,
    unHighlightDomElement: actions.unHighlightDomElement,
    toggleMapScopes: actions.toggleMapScopes,
    setExpandedScope: actions.setExpandedScope,
    addWatchpoint: actions.addWatchpoint,
    removeWatchpoint: actions.removeWatchpoint,
  }
)(Scopes);
