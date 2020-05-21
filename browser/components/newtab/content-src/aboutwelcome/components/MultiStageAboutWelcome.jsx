/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useCallback } from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

export const MultiStageAboutWelcome = props => {
  const [index, setScreenIndex] = useState(0);
  // Transition to next screen, opening about:home on last screen button CTA
  const handleTransition =
    index < props.screens.length
      ? useCallback(() => setScreenIndex(prevState => prevState + 1), [])
      : AboutWelcomeUtils.handleUserAction({
          type: "OPEN_ABOUT_PAGE",
          data: { args: "home", where: "current" },
        });
  return (
    <React.Fragment>
      <div className={`multistageContainer`}>
        {props.screens.map(screen => {
          return index === screen.order ? (
            <WelcomeScreen
              id={screen.id}
              totalNumberOfScreens={props.screens.length}
              order={screen.order}
              content={screen.content}
              navigate={handleTransition}
            />
          ) : null;
        })}
      </div>
    </React.Fragment>
  );
};

export class WelcomeScreen extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleAction = this.handleAction.bind(this);
  }

  handleAction(event) {
    let targetContent = this.props.content[event.target.value];
    if (!(targetContent && targetContent.action)) {
      return;
    }
    if (targetContent.action.type) {
      AboutWelcomeUtils.handleUserAction(targetContent.action);
    }
    if (targetContent.action.navigate) {
      this.props.navigate();
    }
  }

  renderSecondaryCTA(className) {
    return (
      <div className={`secondary-cta ${className}`}>
        <Localized text={this.props.content.secondary_button.text}>
          <span />
        </Localized>
        <Localized text={this.props.content.secondary_button.label}>
          <button
            className="secondary"
            value="secondary_button"
            onClick={this.handleAction}
          />
        </Localized>
      </div>
    );
  }

  renderTiles() {
    return this.props.content.tiles.topSites ? (
      <div className="tiles-section" />
    ) : null;
  }

  renderStepsIndicator() {
    let steps = [];
    for (let i = 0; i < this.props.totalNumberOfScreens; i++) {
      let className = i === this.props.order ? "current" : "";
      steps.push(<div key={i} className={`indicator ${className}`} />);
    }
    return steps;
  }

  render() {
    const { content } = this.props;
    return (
      <main className={`screen ${this.props.id}`}>
        {content.secondary_button && content.secondary_button.position === "top"
          ? this.renderSecondaryCTA("top")
          : null}
        <div className="brand-logo" />
        <div className="welcome-text">
          <Localized text={content.title}>
            <h1 />
          </Localized>
          <Localized text={content.subtitle}>
            <h2 />
          </Localized>
        </div>
        {content.tiles ? this.renderTiles() : null}
        <div>
          <Localized
            text={content.primary_button ? content.primary_button.label : null}
          >
            <button
              className="primary"
              value="primary_button"
              onClick={this.handleAction}
            />
          </Localized>
        </div>
        {content.secondary_button && content.secondary_button.position !== "top"
          ? this.renderSecondaryCTA()
          : null}
        <div className="steps">{this.renderStepsIndicator()}</div>
      </main>
    );
  }
}
