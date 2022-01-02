/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";

import { formatDisplayName } from "../../utils/pause/frames";

import "./PreviewFunction.css";

const IGNORED_SOURCE_URLS = ["debugger eval code"];

export default class PreviewFunction extends Component {
  renderFunctionName(func) {
    const { l10n } = this.context;
    const name = formatDisplayName(func, undefined, l10n);
    return <span className="function-name">{name}</span>;
  }

  renderParams(func) {
    const { parameterNames = [] } = func;

    return parameterNames
      .filter(Boolean)
      .map((param, i, arr) => {
        const elements = [
          <span className="param" key={param}>
            {param}
          </span>,
        ];
        // if this isn't the last param, add a comma
        if (i !== arr.length - 1) {
          elements.push(
            <span className="delimiter" key={i}>
              {", "}
            </span>
          );
        }
        return elements;
      })
      .flat();
  }

  jumpToDefinitionButton(func) {
    const { location } = func;

    if (
      location &&
      location.url &&
      !IGNORED_SOURCE_URLS.includes(location.url)
    ) {
      const lastIndex = location.url.lastIndexOf("/");

      return (
        <button
          className="jump-definition"
          draggable="false"
          title={`${location.url.slice(lastIndex + 1)}:${location.line}`}
        />
      );
    }
  }

  render() {
    const { func } = this.props;
    return (
      <span className="function-signature">
        {this.renderFunctionName(func)}
        <span className="paren">(</span>
        {this.renderParams(func)}
        <span className="paren">)</span>
        {this.jumpToDefinitionButton(func)}
      </span>
    );
  }
}

PreviewFunction.contextTypes = { l10n: PropTypes.object };
