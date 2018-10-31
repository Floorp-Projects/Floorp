/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "react-redux";
import classnames from "classnames";
import Svg from "../shared/Svg";
import actions from "../../actions";
import {
  getSelectedSource,
  getPrettySource,
  getPaneCollapse
} from "../../selectors";

import { features } from "../../utils/prefs";
import {
  isPretty,
  isLoaded,
  getFilename,
  isOriginal,
  isLoading
} from "../../utils/source";
import { getGeneratedSource } from "../../reducers/sources";
import { shouldShowFooter, shouldShowPrettyPrint } from "../../utils/editor";

import { PaneToggleButton } from "../shared/Button";

import type { Source } from "../../types";

import "./Footer.css";

type Props = {
  selectedSource: Source,
  mappedSource: Source,
  endPanelCollapsed: boolean,
  horizontal: boolean,
  togglePrettyPrint: string => void,
  toggleBlackBox: Object => void,
  jumpToMappedLocation: (Source: any) => void,
  recordCoverage: () => void,
  togglePaneCollapse: () => void
};

class SourceFooter extends PureComponent<Props> {
  prettyPrintButton() {
    const { selectedSource, togglePrettyPrint } = this.props;

    if (isLoading(selectedSource) && selectedSource.isPrettyPrinted) {
      return (
        <div className="loader">
          <Svg name="loader" />
        </div>
      );
    }

    if (!shouldShowPrettyPrint(selectedSource)) {
      return;
    }

    const tooltip = L10N.getStr("sourceTabs.prettyPrint");
    const sourceLoaded = selectedSource && isLoaded(selectedSource);

    const type = "prettyPrint";
    return (
      <button
        onClick={() => togglePrettyPrint(selectedSource.id)}
        className={classnames("action", type, {
          active: sourceLoaded,
          pretty: isPretty(selectedSource)
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <img className={type} />
      </button>
    );
  }

  blackBoxButton() {
    const { selectedSource, toggleBlackBox } = this.props;
    const sourceLoaded = selectedSource && isLoaded(selectedSource);

    if (!sourceLoaded || selectedSource.isPrettyPrinted) {
      return;
    }

    const blackboxed = selectedSource.isBlackBoxed;

    const tooltip = L10N.getStr("sourceFooter.blackbox");
    const type = "black-box";

    return (
      <button
        onClick={() => toggleBlackBox(selectedSource)}
        className={classnames("action", type, {
          active: sourceLoaded,
          blackboxed: blackboxed
        })}
        key={type}
        title={tooltip}
        aria-label={tooltip}
      >
        <img className="blackBox" />
      </button>
    );
  }

  blackBoxSummary() {
    const { selectedSource } = this.props;

    if (!selectedSource || !selectedSource.isBlackBoxed) {
      return;
    }

    return (
      <span className="blackbox-summary">
        {L10N.getStr("sourceFooter.blackboxed")}
      </span>
    );
  }

  coverageButton() {
    const { recordCoverage } = this.props;

    if (!features.codeCoverage) {
      return;
    }

    return (
      <button
        className="coverage action"
        title={L10N.getStr("sourceFooter.codeCoverage")}
        onClick={() => recordCoverage()}
        aria-label={L10N.getStr("sourceFooter.codeCoverage")}
      >
        C
      </button>
    );
  }

  renderToggleButton() {
    if (this.props.horizontal) {
      return;
    }

    return (
      <PaneToggleButton
        position="end"
        collapsed={!this.props.endPanelCollapsed}
        horizontal={this.props.horizontal}
        handleClick={this.props.togglePaneCollapse}
      />
    );
  }

  renderCommands() {
    return (
      <div className="commands">
        {this.prettyPrintButton()}
        {this.blackBoxButton()}
        {this.blackBoxSummary()}
        {this.coverageButton()}
      </div>
    );
  }

  renderSourceSummary() {
    const { mappedSource, jumpToMappedLocation, selectedSource } = this.props;

    if (!mappedSource || !isOriginal(selectedSource)) {
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
      column: 1
    };
    return (
      <button
        className="mapped-source"
        onClick={() => jumpToMappedLocation(mappedSourceLocation)}
        title={tooltip}
      >
        <span>{title}</span>
      </button>
    );
  }

  render() {
    const { selectedSource, horizontal } = this.props;

    if (!shouldShowFooter(selectedSource, horizontal)) {
      return null;
    }

    return (
      <div className="source-footer">
        {this.renderCommands()}
        {this.renderSourceSummary()}
        {this.renderToggleButton()}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedId = selectedSource.id;

  return {
    selectedSource,
    mappedSource: getGeneratedSource(state, selectedSource),
    prettySource: getPrettySource(state, selectedId),
    endPanelCollapsed: getPaneCollapse(state, "end")
  };
};

export default connect(
  mapStateToProps,
  {
    togglePrettyPrint: actions.togglePrettyPrint,
    toggleBlackBox: actions.toggleBlackBox,
    jumpToMappedLocation: actions.jumpToMappedLocation,
    recordCoverage: actions.recordCoverage,
    togglePaneCollapse: actions.togglePaneCollapse
  }
)(SourceFooter);
