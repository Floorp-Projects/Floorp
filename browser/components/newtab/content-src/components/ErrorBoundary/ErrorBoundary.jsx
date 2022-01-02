/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { A11yLinkButton } from "content-src/components/A11yLinkButton/A11yLinkButton";
import React from "react";

export class ErrorBoundaryFallback extends React.PureComponent {
  constructor(props) {
    super(props);
    this.windowObj = this.props.windowObj || window;
    this.onClick = this.onClick.bind(this);
  }

  /**
   * Since we only get here if part of the page has crashed, do a
   * forced reload to give us the best chance at recovering.
   */
  onClick() {
    this.windowObj.location.reload(true);
  }

  render() {
    const defaultClass = "as-error-fallback";
    let className;
    if ("className" in this.props) {
      className = `${this.props.className} ${defaultClass}`;
    } else {
      className = defaultClass;
    }

    // "A11yLinkButton" to force normal link styling stuff (eg cursor on hover)
    return (
      <div className={className}>
        <div data-l10n-id="newtab-error-fallback-info" />
        <span>
          <A11yLinkButton
            className="reload-button"
            onClick={this.onClick}
            data-l10n-id="newtab-error-fallback-refresh-link"
          />
        </span>
      </div>
    );
  }
}
ErrorBoundaryFallback.defaultProps = { className: "as-error-fallback" };

export class ErrorBoundary extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { hasError: false };
  }

  componentDidCatch(error, info) {
    this.setState({ hasError: true });
  }

  render() {
    if (!this.state.hasError) {
      return this.props.children;
    }

    return <this.props.FallbackComponent className={this.props.className} />;
  }
}

ErrorBoundary.defaultProps = { FallbackComponent: ErrorBoundaryFallback };
