/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { ModalOverlayWrapper } from "../../components/ModalOverlay/ModalOverlay";
import { addUtmParams } from "../FirstRun/addUtmParams";
import React from "react";

// From resource://devtools/client/shared/focus.js
const FOCUSABLE_SELECTOR = [
  "a[href]:not([tabindex='-1'])",
  "button:not([disabled]):not([tabindex='-1'])",
  "iframe:not([tabindex='-1'])",
  "input:not([disabled]):not([tabindex='-1'])",
  "select:not([disabled]):not([tabindex='-1'])",
  "textarea:not([disabled]):not([tabindex='-1'])",
  "[tabindex]:not([tabindex='-1'])",
].join(", ");

export class Trailhead extends React.PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onStartBlur = this.onStartBlur.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);

    this.state = {
      emailInput: "",
    };
  }

  get dialog() {
    return this.props.document.getElementById("trailheadDialog");
  }

  componentDidMount() {
    // We need to remove hide-main since we should show it underneath everything that has rendered
    this.props.document.body.classList.remove("hide-main");

    // The rest of the page is "hidden" to screen readers when the modal is open
    this.props.document
      .getElementById("root")
      .setAttribute("aria-hidden", "true");
    // Start with focus in the email input box
    const input = this.dialog.querySelector("input[name=email]");
    if (input) {
      input.focus();
    }
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({ emailInput: e.target.value });
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onStartBlur(event) {
    // Make sure focus stays within the dialog when tabbing from the button
    const { dialog } = this;
    if (
      event.relatedTarget &&
      !(
        dialog.compareDocumentPosition(event.relatedTarget) &
        dialog.DOCUMENT_POSITION_CONTAINED_BY
      )
    ) {
      dialog.querySelector(FOCUSABLE_SELECTOR).focus();
    }
  }

  onSubmit(event) {
    // Dynamically require the email on submission so screen readers don't read
    // out it's always required because there's also ways to skip the modal
    const { email } = event.target.elements;
    if (!email.value.length) {
      email.required = true;
      email.checkValidity();
      event.preventDefault();
      return;
    }

    this.props.dispatch(
      ac.UserEvent({ event: "SUBMIT_EMAIL", ...this._getFormInfo() })
    );

    global.addEventListener("visibilitychange", this.closeModal);
  }

  closeModal(ev) {
    global.removeEventListener("visibilitychange", this.closeModal);
    this.props.document.body.classList.remove("welcome");
    this.props.document.getElementById("root").removeAttribute("aria-hidden");
    this.props.onNextScene();

    // If closeModal() was triggered by a visibilitychange event, the user actually
    // submitted the email form so we don't send a SKIPPED_SIGNIN ping.
    if (!ev || ev.type !== "visibilitychange") {
      this.props.dispatch(
        ac.UserEvent({ event: "SKIPPED_SIGNIN", ...this._getFormInfo() })
      );
    }

    // Bug 1190882 - Focus in a disappearing dialog confuses screen readers
    this.props.document.activeElement.blur();
  }

  /**
   * Report to telemetry additional information about the form submission.
   */
  _getFormInfo() {
    const value = { has_flow_params: this.props.flowParams.flowId.length > 0 };
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
    const { props } = this;
    const { UTMTerm } = props;
    const { content } = props.message;
    const innerClassName = ["trailhead", content && content.className]
      .filter(v => v)
      .join(" ");

    return (
      <ModalOverlayWrapper
        innerClassName={innerClassName}
        onClose={this.closeModal}
        id="trailheadDialog"
        headerId="trailheadHeader"
      >
        <div className="trailheadInner">
          <div className="trailheadContent">
            <h1 data-l10n-id={content.title.string_id} id="trailheadHeader" />
            {content.subtitle && (
              <p data-l10n-id={content.subtitle.string_id} />
            )}
            <ul className="trailheadBenefits">
              {content.benefits.map(item => (
                <li key={item.id} className={item.id}>
                  <h3 data-l10n-id={item.title.string_id} />
                  <p data-l10n-id={item.text.string_id} />
                </li>
              ))}
            </ul>
            <a
              className="trailheadLearn"
              data-l10n-id={content.learn.text.string_id}
              href={addUtmParams(content.learn.url, UTMTerm)}
              target="_blank"
              rel="noopener noreferrer"
            />
          </div>
          <div
            role="group"
            aria-labelledby="joinFormHeader"
            aria-describedby="joinFormBody"
            className="trailheadForm"
          >
            <h3
              id="joinFormHeader"
              data-l10n-id={content.form.title.string_id}
            />
            <p id="joinFormBody" data-l10n-id={content.form.text.string_id} />
            <form
              method="get"
              action={this.props.fxaEndpoint}
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
              <input name="utm_source" type="hidden" value="activity-stream" />
              <input name="utm_campaign" type="hidden" value="firstrun" />
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
              <p
                className="trailheadTerms"
                data-l10n-id="onboarding-join-form-legal"
              >
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
              <button
                data-l10n-id={content.form.button.string_id}
                type="submit"
              />
            </form>
          </div>
        </div>

        <button
          className="trailheadStart"
          data-l10n-id={content.skipButton.string_id}
          onBlur={this.onStartBlur}
          onClick={this.closeModal}
        />
      </ModalOverlayWrapper>
    );
  }
}

Trailhead.defaultProps = {
  flowParams: { deviceId: "", flowId: "", flowBeginTime: "" },
};
