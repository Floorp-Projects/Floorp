/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Button } from "../../components/Button/Button";
import React from "react";
import { RichText } from "../../components/RichText/RichText";
import { safeURI } from "../../template-utils";
import { SimpleSnippet } from "../SimpleSnippet/SimpleSnippet";
import { SnippetBase } from "../../components/SnippetBase/SnippetBase";
import ConditionalWrapper from "../../components/ConditionalWrapper/ConditionalWrapper";

// Alt text placeholder in case the prop from the server isn't available
const ICON_ALT_TEXT = "";

export class SubmitFormSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.expandSnippet = this.expandSnippet.bind(this);
    this.handleSubmit = this.handleSubmit.bind(this);
    this.handleSubmitAttempt = this.handleSubmitAttempt.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.state = {
      expanded: false,
      submitAttempted: false,
      signupSubmitted: false,
      signupSuccess: false,
      disableForm: false,
    };
  }

  handleSubmitAttempt() {
    if (!this.state.submitAttempted) {
      this.setState({ submitAttempted: true });
    }
  }

  async handleSubmit(event) {
    let json;

    if (this.state.disableForm) {
      return;
    }

    event.preventDefault();
    this.setState({ disableForm: true });
    this.props.sendUserActionTelemetry({
      event: "CLICK_BUTTON",
      event_context: "conversion-subscribe-activation",
      id: "NEWTAB_FOOTER_BAR_CONTENT",
    });

    if (this.props.form_method.toUpperCase() === "GET") {
      this.props.onBlock({ preventDismiss: true });
      this.refs.form.submit();
      return;
    }

    const { url, formData } = this.props.processFormData
      ? this.props.processFormData(this.refs.mainInput, this.props)
      : { url: this.refs.form.action, formData: new FormData(this.refs.form) };

    try {
      const fetchRequest = new Request(url, {
        body: formData,
        method: "POST",
        credentials: "omit",
      });
      const response = await fetch(fetchRequest); // eslint-disable-line fetch-options/no-fetch-credentials
      json = await response.json();
    } catch (err) {
      console.log(err); // eslint-disable-line no-console
    }

    if (json && json.status === "ok") {
      this.setState({ signupSuccess: true, signupSubmitted: true });
      if (!this.props.content.do_not_autoblock) {
        this.props.onBlock({ preventDismiss: true });
      }
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        event_context: "subscribe-success",
        id: "NEWTAB_FOOTER_BAR_CONTENT",
      });
    } else {
      // eslint-disable-next-line no-console
      console.error(
        "There was a problem submitting the form",
        json || "[No JSON response]"
      );
      this.setState({ signupSuccess: false, signupSubmitted: true });
      this.props.sendUserActionTelemetry({
        event: "CLICK_BUTTON",
        event_context: "subscribe-error",
        id: "NEWTAB_FOOTER_BAR_CONTENT",
      });
    }

    this.setState({ disableForm: false });
  }

  expandSnippet() {
    this.props.sendUserActionTelemetry({
      event: "CLICK_BUTTON",
      event_context: "scene1-button-learn-more",
      id: this.props.UISurface,
    });

    this.setState({
      expanded: true,
      signupSuccess: false,
      signupSubmitted: false,
    });
  }

  renderHiddenFormInputs() {
    const { hidden_inputs } = this.props.content;

    if (!hidden_inputs) {
      return null;
    }

    return Object.keys(hidden_inputs).map((key, idx) => (
      <input key={idx} type="hidden" name={key} value={hidden_inputs[key]} />
    ));
  }

  renderDisclaimer() {
    const { content } = this.props;
    if (!content.scene2_disclaimer_html) {
      return null;
    }
    return (
      <p className="disclaimerText">
        <RichText
          text={content.scene2_disclaimer_html}
          localization_id="disclaimer_html"
          links={content.links}
          doNotAutoBlock={true}
          openNewWindow={true}
          sendClick={this.props.sendClick}
        />
      </p>
    );
  }

  renderFormPrivacyNotice() {
    const { content } = this.props;
    if (!content.scene2_privacy_html) {
      return null;
    }
    return (
      <p className="privacyNotice">
        <input
          type="checkbox"
          id="id_privacy"
          name="privacy"
          required="required"
        />
        <label htmlFor="id_privacy">
          <RichText
            text={content.scene2_privacy_html}
            localization_id="privacy_html"
            links={content.links}
            doNotAutoBlock={true}
            openNewWindow={true}
            sendClick={this.props.sendClick}
          />
        </label>
      </p>
    );
  }

  renderSignupSubmitted() {
    const { content } = this.props;
    const isSuccess = this.state.signupSuccess;
    const successTitle = isSuccess && content.success_title;
    const bodyText = isSuccess
      ? { success_text: content.success_text }
      : { error_text: content.error_text };
    const retryButtonText = content.retry_button_label;
    return (
      <SnippetBase {...this.props}>
        <div className="submissionStatus">
          {successTitle ? (
            <h2 className="submitStatusTitle">{successTitle}</h2>
          ) : null}
          <p>
            <RichText
              {...bodyText}
              localization_id={isSuccess ? "success_text" : "error_text"}
            />
            {isSuccess ? null : (
              <Button onClick={this.expandSnippet}>{retryButtonText}</Button>
            )}
          </p>
        </div>
      </SnippetBase>
    );
  }

  onInputChange(event) {
    if (!this.props.validateInput) {
      return;
    }
    const hasError = this.props.validateInput(
      event.target.value,
      this.props.content
    );
    event.target.setCustomValidity(hasError);
  }

  wrapSectionHeader(url) {
    return function(children) {
      return <a href={url}>{children}</a>;
    };
  }

  renderInput() {
    const placholder =
      this.props.content.scene2_email_placeholder_text ||
      this.props.content.scene2_input_placeholder;
    return (
      <input
        ref="mainInput"
        type={this.props.inputType || "email"}
        className={`mainInput${this.state.submitAttempted ? "" : " clean"}`}
        name="email"
        required={true}
        placeholder={placholder}
        onChange={this.props.validateInput ? this.onInputChange : null}
      />
    );
  }

  renderForm() {
    return (
      <form
        action={this.props.form_action}
        method={this.props.form_method}
        onSubmit={this.handleSubmit}
        ref="form"
      >
        {this.renderHiddenFormInputs()}
        <div>
          {this.renderInput()}
          <button
            type="submit"
            className="ASRouterButton primary"
            onClick={this.handleSubmitAttempt}
            ref="formSubmitBtn"
          >
            {this.props.content.scene2_button_label}
          </button>
        </div>
        {this.renderFormPrivacyNotice() || this.renderDisclaimer()}
      </form>
    );
  }

  renderScene2Icon() {
    const { content } = this.props;
    if (!content.scene2_icon) {
      return null;
    }

    return (
      <div className="scene2Icon">
        <img
          src={safeURI(content.scene2_icon)}
          className="icon-light-theme"
          alt={content.scene2_icon_alt_text || ICON_ALT_TEXT}
        />
        <img
          src={safeURI(content.scene2_icon_dark_theme || content.scene2_icon)}
          className="icon-dark-theme"
          alt={content.scene2_icon_alt_text || ICON_ALT_TEXT}
        />
      </div>
    );
  }

  renderSignupView() {
    const { content } = this.props;
    const containerClass = `SubmitFormSnippet ${this.props.className}`;
    return (
      <SnippetBase
        {...this.props}
        className={containerClass}
        footerDismiss={true}
      >
        {this.renderScene2Icon()}
        <div className="message">
          <p>
            {content.scene2_title && (
              <h3 className="scene2Title">{content.scene2_title}</h3>
            )}{" "}
            {content.scene2_text && (
              <RichText
                scene2_text={content.scene2_text}
                localization_id="scene2_text"
              />
            )}
          </p>
        </div>
        {this.renderForm()}
      </SnippetBase>
    );
  }

  renderSectionHeader() {
    const { props } = this;

    // an icon and text must be specified to render the section header
    if (props.content.section_title_icon && props.content.section_title_text) {
      const sectionTitleIconLight = safeURI(props.content.section_title_icon);
      const sectionTitleIconDark = safeURI(
        props.content.section_title_icon_dark_theme ||
          props.content.section_title_icon
      );
      const sectionTitleURL = props.content.section_title_url;

      return (
        <div className="section-header">
          <h3 className="section-title">
            <ConditionalWrapper
              wrap={this.wrapSectionHeader(sectionTitleURL)}
              condition={sectionTitleURL}
            >
              <span
                className="icon icon-small-spacer icon-light-theme"
                style={{ backgroundImage: `url("${sectionTitleIconLight}")` }}
              />
              <span
                className="icon icon-small-spacer icon-dark-theme"
                style={{ backgroundImage: `url("${sectionTitleIconDark}")` }}
              />
              <span className="section-title-text">
                {props.content.section_title_text}
              </span>
            </ConditionalWrapper>
          </h3>
        </div>
      );
    }

    return null;
  }

  renderSignupViewAlt() {
    const { content } = this.props;
    const containerClass = `SubmitFormSnippet ${this.props.className} scene2Alt`;
    return (
      <SnippetBase
        {...this.props}
        className={containerClass}
        // Don't show bottom dismiss button
        footerDismiss={false}
      >
        {this.renderSectionHeader()}
        {this.renderScene2Icon()}
        <div className="message">
          <p>
            {content.scene2_text && (
              <RichText
                scene2_text={content.scene2_text}
                localization_id="scene2_text"
              />
            )}
          </p>
          {this.renderForm()}
        </div>
      </SnippetBase>
    );
  }

  getFirstSceneContent() {
    return Object.keys(this.props.content)
      .filter(key => key.includes("scene1"))
      .reduce((acc, key) => {
        acc[key.substr(7)] = this.props.content[key];
        return acc;
      }, {});
  }

  render() {
    const content = { ...this.props.content, ...this.getFirstSceneContent() };

    if (this.state.signupSubmitted) {
      return this.renderSignupSubmitted();
    }
    // Render only scene 2 (signup view). Must check before `renderSignupView`
    // to catch the Failure/Try again scenario where we want to return and render
    // the scene again.
    if (this.props.expandedAlt) {
      return this.renderSignupViewAlt();
    }
    if (this.state.expanded) {
      return this.renderSignupView();
    }
    return (
      <SimpleSnippet
        {...this.props}
        content={content}
        onButtonClick={this.expandSnippet}
      />
    );
  }
}
