/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { isGeneratedId } from "devtools-source-map";
import { connect } from "../../utils/connect";
import { features } from "../../utils/prefs";
import actions from "../../actions";
import { createObjectClient } from "../../client/firefox";

import {
  getSelectedSource,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getIsPaused,
  getPauseReason,
  getMapScopes,
  getCurrentThread
} from "../../selectors";
import { getScopes } from "../../utils/pause/scopes";

import { objectInspector } from "devtools-reps";
import AccessibleImage from "../shared/AccessibleImage";

import type { Why } from "../../types";
import type { NamedValue } from "../../utils/pause/scopes/types";

import "./Scopes.css";

const mdnLink =
  "https://developer.mozilla.org/en-US/docs/Tools/Debugger/Using_the_Debugger_map_scopes_feature";

const { ObjectInspector } = objectInspector;

type Props = {
  isPaused: boolean,
  selectedFrame: Object,
  generatedFrameScopes: Object,
  originalFrameScopes: Object | null,
  isLoading: boolean,
  why: Why,
  shouldMapScopes: boolean,
  openLink: typeof actions.openLink,
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  toggleMapScopes: typeof actions.toggleMapScopes
};

type State = {
  originalScopes: ?(NamedValue[]),
  generatedScopes: ?(NamedValue[]),
  showOriginal: boolean
};

class Scopes extends PureComponent<Props, State> {
  constructor(props: Props, ...args) {
    const {
      why,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes
    } = props;

    super(props, ...args);

    this.state = {
      originalScopes: getScopes(why, selectedFrame, originalFrameScopes),
      generatedScopes: getScopes(why, selectedFrame, generatedFrameScopes),
      showOriginal: true
    };
  }

  componentWillReceiveProps(nextProps) {
    const {
      isPaused,
      selectedFrame,
      originalFrameScopes,
      generatedFrameScopes
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
        )
      });
    }
  }

  onToggleMapScopes = () => {
    this.props.toggleMapScopes();
  };

  renderMapScopes() {
    const { selectedFrame, shouldMapScopes } = this.props;

    if (
      !features.mapScopes ||
      !selectedFrame ||
      isGeneratedId(selectedFrame.location.sourceId)
    ) {
      return null;
    }

    return (
      <div className="toggle-map-scopes" onClick={this.onToggleMapScopes}>
        <input
          type="checkbox"
          checked={shouldMapScopes ? "checked" : ""}
          onChange={e => e.stopPropagation() && this.onToggleMapScopes()}
        />
        <div className="toggle-map-scopes-label">
          <span>{L10N.getStr("scopes.mapScopes")}</span>
        </div>
        <a className="mdn" target="_blank" href={mdnLink}>
          <AccessibleImage className="shortcuts" />
        </a>
      </div>
    );
  }

  renderScopesList() {
    const {
      isPaused,
      isLoading,
      openLink,
      openElementInInspector,
      shouldMapScopes
    } = this.props;
    const { originalScopes, generatedScopes, showOriginal } = this.state;

    const scopes =
      (showOriginal && shouldMapScopes && originalScopes) || generatedScopes;

    if (scopes && !isLoading) {
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
          />
          {originalScopes && shouldMapScopes ? (
            <div className="scope-type-toggle">
              <button
                onClick={e => {
                  e.preventDefault();
                  this.setState({ showOriginal: !showOriginal });
                }}
              >
                {showOriginal
                  ? L10N.getStr("scopes.toggleToGenerated")
                  : L10N.getStr("scopes.toggleToOriginal")}
              </button>
            </div>
          ) : null}
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
    return (
      <div className="scopes-content">
        {this.renderMapScopes()}
        {this.renderScopesList()}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);
  const selectedSource = getSelectedSource(state);

  const {
    scope: originalFrameScopes,
    pending: originalPending
  } = getOriginalFrameScope(
    state,
    thread,
    selectedSource && selectedSource.id,
    selectedFrame && selectedFrame.id
  ) || { scope: null, pending: false };

  const {
    scope: generatedFrameScopes,
    pending: generatedPending
  } = getGeneratedFrameScope(
    state,
    thread,
    selectedFrame && selectedFrame.id
  ) || {
    scope: null,
    pending: false
  };

  return {
    selectedFrame,
    shouldMapScopes: getMapScopes(state),
    isPaused: getIsPaused(state, thread),
    isLoading: generatedPending || originalPending,
    why: getPauseReason(state, thread),
    originalFrameScopes,
    generatedFrameScopes
  };
};

export default connect(
  mapStateToProps,
  {
    openLink: actions.openLink,
    openElementInInspector: actions.openElementInInspectorCommand,
    toggleMapScopes: actions.toggleMapScopes
  }
)(Scopes);
