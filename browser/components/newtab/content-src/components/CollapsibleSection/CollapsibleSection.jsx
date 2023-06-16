/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ErrorBoundary } from "content-src/components/ErrorBoundary/ErrorBoundary";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";
import React from "react";
import { connect } from "react-redux";

/**
 * A section that can collapse. As of bug 1710937, it can no longer collapse.
 * See bug 1727365 for follow-up work to simplify this component.
 */
export class _CollapsibleSection extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBodyMount = this.onBodyMount.bind(this);
    this.onMenuButtonMouseEnter = this.onMenuButtonMouseEnter.bind(this);
    this.onMenuButtonMouseLeave = this.onMenuButtonMouseLeave.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.state = {
      menuButtonHover: false,
      showContextMenu: false,
    };
    this.setContextMenuButtonRef = this.setContextMenuButtonRef.bind(this);
  }

  setContextMenuButtonRef(element) {
    this.contextMenuButtonRef = element;
  }

  onBodyMount(node) {
    this.sectionBody = node;
  }

  onMenuButtonMouseEnter() {
    this.setState({ menuButtonHover: true });
  }

  onMenuButtonMouseLeave() {
    this.setState({ menuButtonHover: false });
  }

  onMenuUpdate(showContextMenu) {
    this.setState({ showContextMenu });
  }

  render() {
    const { isAnimating, maxHeight, menuButtonHover, showContextMenu } =
      this.state;
    const { id, collapsed, learnMore, title, subTitle } = this.props;
    const active = menuButtonHover || showContextMenu;
    let bodyStyle;
    if (isAnimating && !collapsed) {
      bodyStyle = { maxHeight };
    } else if (!isAnimating && collapsed) {
      bodyStyle = { display: "none" };
    }
    let titleStyle;
    if (this.props.hideTitle) {
      titleStyle = { visibility: "hidden" };
    }
    const hasSubtitleClassName = subTitle ? `has-subtitle` : ``;
    return (
      <section
        className={`collapsible-section ${this.props.className}${
          active ? " active" : ""
        }`}
        // Note: data-section-id is used for web extension api tests in mozilla central
        data-section-id={id}
      >
        <div className="section-top-bar">
          <h3
            className={`section-title-container ${hasSubtitleClassName}`}
            style={titleStyle}
          >
            <span className="section-title">
              <FluentOrText message={title} />
            </span>
            <span className="learn-more-link-wrapper">
              {learnMore && (
                <span className="learn-more-link">
                  <FluentOrText message={learnMore.link.message}>
                    <a href={learnMore.link.href} />
                  </FluentOrText>
                </span>
              )}
            </span>
            {subTitle && (
              <span className="section-sub-title">
                <FluentOrText message={subTitle} />
              </span>
            )}
          </h3>
        </div>
        <ErrorBoundary className="section-body-fallback">
          <div ref={this.onBodyMount} style={bodyStyle}>
            {this.props.children}
          </div>
        </ErrorBoundary>
      </section>
    );
  }
}

_CollapsibleSection.defaultProps = {
  document: global.document || {
    addEventListener: () => {},
    removeEventListener: () => {},
    visibilityState: "hidden",
  },
};

export const CollapsibleSection = connect(state => ({
  Prefs: state.Prefs,
}))(_CollapsibleSection);
