/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";

export class StartupOverlay extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onInputChange = this.onInputChange.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.clickSkip = this.clickSkip.bind(this);
    this.removeOverlay = this.removeOverlay.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);

    this.utmParams =
      "utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-control";

    this.state = {
      show: false,
      emailInput: "",
    };
  }

  componentWillMount() {
    global.document.body.classList.add("fxa");
  }

  componentDidMount() {
    // Timeout to allow the scene to render once before attaching the attribute
    // to trigger the animation.
    setTimeout(() => {
      this.setState({ show: true });
    }, 10);
    // Hide the page content from screen readers while the modal is open
    this.props.document
      .getElementById("root")
      .setAttribute("aria-hidden", "true");
  }

  removeOverlay() {
    window.removeEventListener("visibilitychange", this.removeOverlay);
    document.body.classList.remove("hide-main", "fxa");
    this.setState({ show: false });
    // Re-enable the document for screen readers
    this.props.document
      .getElementById("root")
      .setAttribute("aria-hidden", "false");

    setTimeout(() => {
      // Allow scrolling and fully remove overlay after animation finishes.
      this.props.onBlock();
      document.body.classList.remove("welcome");
    }, 400);
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({ emailInput: e.target.value });
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onSubmit() {
    this.props.dispatch(
      ac.UserEvent({ event: "SUBMIT_EMAIL", ...this._getFormInfo() })
    );

    window.addEventListener("visibilitychange", this.removeOverlay);
  }

  clickSkip() {
    this.props.dispatch(
      ac.UserEvent({ event: "SKIPPED_SIGNIN", ...this._getFormInfo() })
    );
    this.removeOverlay();
  }

  /**
   * Report to telemetry additional information about the form submission.
   */
  _getFormInfo() {
    const value = {
      has_flow_params: this.props.flowParams.flowId.length > 0,
    };
    return { value };
  }

  onInputInvalid(e) {
    let error = e.target.previousSibling;
    error.classList.add("active");
    e.target.classList.add("invalid");
    e.preventDefault(); // Override built-in form validation popup
    e.target.focus();
  }

  render() {
    return (
      <div className={`overlay-wrapper ${this.state.show ? "show" : ""}`}>
        <div className="background" />
        <div className="firstrun-scene">
          <div className="fxaccounts-container">
            <div className="firstrun-left-divider">
              <h1
                className="firstrun-title"
                data-l10n-id="onboarding-sync-welcome-header"
              />
              <p
                className="firstrun-content"
                data-l10n-id="onboarding-sync-welcome-content"
              />
              <a
                className="firstrun-link"
                href={`https://www.mozilla.org/firefox/features/sync/?${
                  this.utmParams
                }`}
                target="_blank"
                rel="noopener noreferrer"
                data-l10n-id="onboarding-sync-welcome-learn-more-link"
              />
            </div>
            <div className="firstrun-sign-in">
              <p className="form-header">
                <span data-l10n-id="onboarding-sync-form-header" />
                <span
                  className="sub-header"
                  data-l10n-id="onboarding-sync-form-sub-header"
                />
              </p>
              <form
                method="get"
                action={this.props.fxa_endpoint}
                target="_blank"
                rel="noopener noreferrer"
                onSubmit={this.onSubmit}
              >
                <input name="service" type="hidden" value="sync" />
                <input name="action" type="hidden" value="email" />
                <input name="context" type="hidden" value="fx_desktop_v3" />
                <input
                  name="entrypoint"
                  type="hidden"
                  value="activity-stream-firstrun"
                />
                <input
                  name="utm_source"
                  type="hidden"
                  value="activity-stream"
                />
                <input name="utm_campaign" type="hidden" value="firstrun" />
                <input name="utm_medium" type="hidden" value="referral" />
                <input
                  name="utm_term"
                  type="hidden"
                  value="trailhead-control"
                />
                <input
                  name="device_id"
                  type="hidden"
                  value={this.props.flowParams.deviceId}
                />
                <input
                  name="flow_id"
                  type="hidden"
                  value={this.props.flowParams.flowId}
                />
                <input
                  name="flow_begin_time"
                  type="hidden"
                  value={this.props.flowParams.flowBeginTime}
                />
                <span
                  className="error"
                  data-l10n-id="onboarding-sync-form-invalid-input"
                />
                <input
                  className="email-input"
                  name="email"
                  type="email"
                  required={true}
                  onInvalid={this.onInputInvalid}
                  onChange={this.onInputChange}
                  data-l10n-id="onboarding-sync-form-input"
                />
                <div className="extra-links">
                  <p data-l10n-id="onboarding-sync-legal-notice">
                    <a
                      data-l10n-name="terms"
                      target="_blank"
                      rel="noopener noreferrer"
                      href={`${this.props.fxa_endpoint}/legal/terms?${
                        this.utmParams
                      }`}
                    />
                    <a
                      data-l10n-name="privacy"
                      target="_blank"
                      rel="noopener noreferrer"
                      href={`${this.props.fxa_endpoint}/legal/privacy?${
                        this.utmParams
                      }`}
                    />
                  </p>
                </div>
                <button
                  className="continue-button"
                  type="submit"
                  data-l10n-id="onboarding-sync-form-continue-button"
                />
              </form>
              <button
                className="skip-button"
                disabled={!!this.state.emailInput}
                onClick={this.clickSkip}
                data-l10n-id="onboarding-sync-form-skip-login-button"
              />
            </div>
          </div>
        </div>
      </div>
    );
  }
}

StartupOverlay.defaultProps = {
  flowParams: { deviceId: "", flowId: "", flowBeginTime: "" },
};
