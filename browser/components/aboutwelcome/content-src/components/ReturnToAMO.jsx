/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import {
  AboutWelcomeUtils,
  DEFAULT_RTAMO_CONTENT,
} from "../lib/aboutwelcome-utils";
import { MultiStageProtonScreen } from "./MultiStageProtonScreen";
import { BASE_PARAMS } from "../../../newtab/content-src/asrouter/templates/FirstRun/addUtmParams";

export class ReturnToAMO extends React.PureComponent {
  constructor(props) {
    super(props);
    this.fetchFlowParams = this.fetchFlowParams.bind(this);
    this.handleAction = this.handleAction.bind(this);
  }

  async fetchFlowParams() {
    if (this.props.metricsFlowUri) {
      this.setState({
        flowParams: await AboutWelcomeUtils.fetchFlowParams(
          this.props.metricsFlowUri
        ),
      });
    }
  }

  componentDidUpdate() {
    this.fetchFlowParams();
  }

  handleAction(event) {
    const { content, message_id, url, utm_term } = this.props;
    let { action, source_id } = content[event.currentTarget.value];
    let { type, data } = action;

    if (type === "INSTALL_ADDON_FROM_URL") {
      if (!data) {
        return;
      }
      // Set add-on url in action.data.url property from JSON
      data = { ...data, url };
    } else if (type === "SHOW_FIREFOX_ACCOUNTS") {
      let params = {
        ...BASE_PARAMS,
        utm_term: `aboutwelcome-${utm_term}-screen`,
      };
      if (action.addFlowParams && this.state.flowParams) {
        params = {
          ...params,
          ...this.state.flowParams,
        };
      }
      data = { ...data, extraParams: params };
    }

    AboutWelcomeUtils.handleUserAction({ type, data });
    AboutWelcomeUtils.sendActionTelemetry(message_id, source_id);
  }

  render() {
    const { content, type } = this.props;

    if (!content) {
      return null;
    }

    if (content?.primary_button.label) {
      content.primary_button.label.string_id = type.includes("theme")
        ? "return-to-amo-add-theme-label"
        : "mr1-return-to-amo-add-extension-label";
    }

    // For experiments, when needed below rendered UI allows settings hard coded strings
    // directly inside JSON except for ReturnToAMOText which picks add-on name and icon from fluent string
    return (
      <div
        className={"outer-wrapper onboardingContainer proton"}
        style={content.backdrop ? { background: content.backdrop } : {}}
      >
        <MultiStageProtonScreen
          content={content}
          isRtamo={true}
          isTheme={type.includes("theme")}
          id={this.props.message_id}
          order={this.props.order || 0}
          totalNumberOfScreens={1}
          isSingleScreen={true}
          autoAdvance={this.props.auto_advance}
          iconURL={
            type.includes("theme")
              ? this.props.themeScreenshots[0]?.url
              : this.props.iconURL
          }
          addonName={this.props.name}
          handleAction={this.handleAction}
        />
      </div>
    );
  }
}

ReturnToAMO.defaultProps = DEFAULT_RTAMO_CONTENT;
