/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "react-redux";
import actions from "../../actions";
import { createObjectClient } from "../../client/firefox";

import {
  getSelectedSource,
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
  isPaused as getIsPaused,
  getPauseReason
} from "../../selectors";
import { getScopes } from "../../utils/pause/scopes";

import { objectInspector } from "devtools-reps";
import type { Pause, Why, Grip } from "../../types";
import type { NamedValue } from "../../utils/pause/scopes/types";

import "./Scopes.css";

const { ObjectInspector } = objectInspector;

type Props = {
  isPaused: Pause,
  selectedFrame: Object,
  generatedFrameScopes: Object,
  originalFrameScopes: Object | null,
  isLoading: boolean,
  why: Why,
  openLink: string => void,
  openElementInInspector: (grip: Grip) => void
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

  render() {
    const {
      isPaused,
      isLoading,
      openLink,
      openElementInInspector
    } = this.props;
    const { originalScopes, generatedScopes, showOriginal } = this.state;

    const scopes = (showOriginal && originalScopes) || generatedScopes;

    if (scopes && !isLoading) {
      return (
        <div className="pane scopes-list">
          <ObjectInspector
            roots={scopes}
            autoExpandAll={false}
            autoExpandDepth={1}
            disableWrap={true}
            focusable={false}
            dimTopLevelWindow={true}
            openLink={openLink}
            createObjectClient={grip => createObjectClient(grip)}
            onDOMNodeClick={grip => openElementInInspector(grip)}
            onInspectIconClick={grip => openElementInInspector(grip)}
          />
          {originalScopes ? (
            <div className="scope-type-toggle">
              <a
                href=""
                onClick={e => {
                  e.preventDefault();
                  this.setState({ showOriginal: !showOriginal });
                }}
              >
                {showOriginal
                  ? L10N.getStr("scopes.toggleToGenerated")
                  : L10N.getStr("scopes.toggleToOriginal")}
              </a>
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
}

const mapStateToProps = state => {
  const selectedFrame = getSelectedFrame(state);
  const selectedSource = getSelectedSource(state);

  const {
    scope: originalFrameScopes,
    pending: originalPending
  } = getOriginalFrameScope(
    state,
    selectedSource && selectedSource.id,
    selectedFrame && selectedFrame.id
  ) || { scope: null, pending: false };

  const {
    scope: generatedFrameScopes,
    pending: generatedPending
  } = getGeneratedFrameScope(state, selectedFrame && selectedFrame.id) || {
    scope: null,
    pending: false
  };

  return {
    selectedFrame,
    isPaused: getIsPaused(state),
    isLoading: generatedPending || originalPending,
    why: getPauseReason(state),
    originalFrameScopes,
    generatedFrameScopes
  };
};

export default connect(
  mapStateToProps,
  {
    openLink: actions.openLink,
    openElementInInspector: actions.openElementInInspectorCommand
  }
)(Scopes);
