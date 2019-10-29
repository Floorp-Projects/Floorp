/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import {
  addUtmParams,
  BASE_PARAMS,
} from "../../templates/FirstRun/addUtmParams";
import React from "react";

export class FxASignupForm extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onSubmit = this.onSubmit.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);
    this.handleSignIn = this.handleSignIn.bind(this);

    this.state = {
      emailInput: "",
    };
  }

  get email() {
    return this.props.document
      .getElementById("fxaSignupForm")
      .querySelector("input[name=email]");
  }

  onSubmit(event) {
    let userEvent = "SUBMIT_EMAIL";
    const { email } = event.target.elements;
    if (email.disabled) {
      userEvent = "SUBMIT_SIGNIN";
    } else if (!email.value.length) {
      email.required = true;
      email.checkValidity();
      event.preventDefault();
      return;
    }

    // Report to telemetry additional information about the form submission.
    const value = { has_flow_params: !!this.props.flowParams.flowId.length };
    this.props.dispatch(ac.UserEvent({ event: userEvent, value }));

    global.addEventListener("visibilitychange", this.props.onClose);
  }

  handleSignIn(event) {
    // Set disabled to prevent email from appearing in url resulting in the wrong page
    this.email.disabled = true;
  }

  componentDidMount() {
    // Start with focus in the email input box
    if (this.email) {
      this.email.focus();
    }
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({ emailInput: e.target.value });
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onInputInvalid(e) {
    let error = e.target.previousSibling;
    error.classList.add("active");
    e.target.classList.add("invalid");
    e.preventDefault(); // Override built-in form validation popup
    e.target.focus();
  }

  render() {
    const { content, UTMTerm } = this.props;
    return (
      <div
        id="fxaSignupForm"
        role="group"
        aria-labelledby="joinFormHeader"
        aria-describedby="joinFormBody"
        className="fxaSignupForm"
      >
        <h3 id="joinFormHeader" data-l10n-id={content.form.title.string_id} />
        <p id="joinFormBody" data-l10n-id={content.form.text.string_id} />
        <form
          method="get"
          action={this.props.fxaEndpoint}
          target="_blank"
          rel="noopener noreferrer"
          onSubmit={this.onSubmit}
        >
          <input name="action" type="hidden" value="email" />
          <input name="context" type="hidden" value="fx_desktop_v3" />
          <input
            name="entrypoint"
            type="hidden"
            value="activity-stream-firstrun"
          />
          <input name="utm_source" type="hidden" value="activity-stream" />
          <input
            name="utm_campaign"
            type="hidden"
            value={BASE_PARAMS.utm_campaign}
          />
          <input name="utm_term" type="hidden" value={UTMTerm} />
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
          <input name="style" type="hidden" value="trailhead" />
          <p
            data-l10n-id="onboarding-join-form-email-error"
            className="error"
          />
          <input
            data-l10n-id={content.form.email.string_id}
            name="email"
            type="email"
            onInvalid={this.onInputInvalid}
            onChange={this.onInputChange}
          />
          <p className="fxa-terms" data-l10n-id="onboarding-join-form-legal">
            <a
              data-l10n-name="terms"
              target="_blank"
              rel="noopener noreferrer"
              href={addUtmParams(
                "https://accounts.firefox.com/legal/terms",
                UTMTerm
              )}
            />
            <a
              data-l10n-name="privacy"
              target="_blank"
              rel="noopener noreferrer"
              href={addUtmParams(
                "https://accounts.firefox.com/legal/privacy",
                UTMTerm
              )}
            />
          </p>
          <button data-l10n-id={content.form.button.string_id} type="submit" />
          {this.props.showSignInLink && (
            <div className="fxa-signin">
              <span data-l10n-id="onboarding-join-form-signin-label" />
              <button
                data-l10n-id="onboarding-join-form-signin"
                onClick={this.handleSignIn}
              />
            </div>
          )}
        </form>
      </div>
    );
  }
}

FxASignupForm.defaultProps = { document: global.document };
