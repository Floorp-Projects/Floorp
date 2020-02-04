/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { ModalOverlayWrapper } from "../../components/ModalOverlay/ModalOverlay";
import { FxASignupForm } from "../../components/FxASignupForm/FxASignupForm";
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
    this.onStartBlur = this.onStartBlur.bind(this);
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
    const value = { has_flow_params: !!this.props.flowParams.flowId.length };
    return { value };
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
        hasDismissIcon={true}
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
                  <h2 data-l10n-id={item.title.string_id} />
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
          <div className="trailhead-join-form">
            <FxASignupForm
              document={this.props.document}
              content={content}
              dispatch={this.props.dispatch}
              fxaEndpoint={this.props.fxaEndpoint}
              UTMTerm={UTMTerm}
              flowParams={this.props.flowParams}
              onClose={this.closeModal}
              showSignInLink={true}
            />
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
