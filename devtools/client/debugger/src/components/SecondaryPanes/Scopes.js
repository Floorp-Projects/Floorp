/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "../../utils/connect";
import actions from "../../actions";
import { createObjectClient } from "../../client/firefox";

import {
  getSelectedSource,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getIsPaused,
  getPauseReason,
  isMapScopesEnabled,
  getCurrentThread,
} from "../../selectors";
import { getScopes } from "../../utils/pause/scopes";

// eslint-disable-next-line import/named
import { objectInspector } from "devtools-reps";

import type { Why } from "../../types";
import type { NamedValue } from "../../utils/pause/scopes/types";

import "./Scopes.css";

const { ObjectInspector } = objectInspector;

type Props = {
  isPaused: boolean,
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
      isPaused,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes,
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

  renderScopesList() {
    const {
      isPaused,
      isLoading,
      openLink,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
      mapScopesEnabled,
    } = this.props;
    const { originalScopes, generatedScopes, showOriginal } = this.state;

    const scopes =
      (showOriginal && mapScopesEnabled && originalScopes) || generatedScopes;

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
          />
        </div>
      );
    }

    let stateText = L10N.getStr("scopes.notPaused");
    if (isPaused) {
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
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);
  const selectedSource = getSelectedSource(state);

  const {
    scope: originalFrameScopes,
    pending: originalPending,
  } = getOriginalFrameScope(
    state,
    thread,
    selectedSource && selectedSource.id,
    selectedFrame && selectedFrame.id
  ) || { scope: null, pending: false };

  const {
    scope: generatedFrameScopes,
    pending: generatedPending,
  } = getGeneratedFrameScope(
    state,
    thread,
    selectedFrame && selectedFrame.id
  ) || {
    scope: null,
    pending: false,
  };

  return {
    selectedFrame,
    mapScopesEnabled: isMapScopesEnabled(state),
    isPaused: getIsPaused(state, thread),
    isLoading: generatedPending || originalPending,
    why: getPauseReason(state, thread),
    originalFrameScopes,
    generatedFrameScopes,
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
  }
)(Scopes);
