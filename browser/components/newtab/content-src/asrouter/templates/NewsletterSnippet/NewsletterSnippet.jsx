import React from "react";
import {SimpleSnippet} from "../SimpleSnippet/SimpleSnippet";
import {SnippetBase} from "../../components/SnippetBase/SnippetBase";

export class NewsletterSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.expandSnippet = this.expandSnippet.bind(this);
    this.handleSubmit = this.handleSubmit.bind(this);
    this.state = {
      expanded: false,
      signupSubmitted: false,
      signupSuccess: false,
    };
  }

  async handleSubmit(event) {
    let json;
    const fetchConfig = {
      body: new FormData(this.refs.newsletterForm),
      method: "POST",
    };

    event.preventDefault();
    this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", value: "conversion-subscribe-activation", id: "NEWTAB_FOOTER_BAR_CONTENT"});

    try {
      const fetchRequest = new Request(this.refs.newsletterForm.action, fetchConfig);
      const response = await fetch(fetchRequest);
      json = await response.json();
    } catch (err) {
      console.log(err); // eslint-disable-line no-console
    }
    if (json && json.status === "ok") {
      this.setState({signupSuccess: true, signupSubmitted: true});
      this.props.onBlock({preventDismiss: true});
      this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", value: "subscribe-success", id: "NEWTAB_FOOTER_BAR_CONTENT"});
    } else {
      this.setState({signupSuccess: false, signupSubmitted: true});
      this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", value: "subscribe-error", id: "NEWTAB_FOOTER_BAR_CONTENT"});
    }
  }

  expandSnippet() {
    this.setState({
      expanded: true,
      signupSuccess: false,
      signupSubmitted: false,
    });
  }

  renderHiddenFormInputs() {
    const {hidden_inputs} = this.props.content;

    if (!hidden_inputs) {
      return null;
    }

    return Object.keys(hidden_inputs).map((key, idx) => <input key={idx} type="hidden" name={key} value={hidden_inputs[key]} />);
  }

  renderFormPrivacyNotice() {
    return (<label className="privacy-notice" htmlFor="id_privacy">
        <p>
          <input type="checkbox" id="id_privacy" name="privacy" required="required" />
          <span>{this.props.privacyNoticeRichText}</span>
        </p>
      </label>);
  }

  renderSignupSubmitted() {
    const message = this.state.signupSuccess ? this.props.content.success_text : this.props.content.error_text;
    const onButtonClick = !this.state.signupSuccess ? this.expandSnippet : null;

    return (<SimpleSnippet className={this.props.className}
      onButtonClick={onButtonClick}
      provider={this.props.provider}
      content={{button_label: this.props.content.button_label, text: message}} />);
  }

  renderSignupView() {
    const {content} = this.props;

    return (<SnippetBase {...this.props} className="NewsletterSnippet" footerDismiss={true}>
        <div className="message">
          <p>{content.scene2_text}</p>
        </div>
        <form action={content.form_action} method="POST" onSubmit={this.handleSubmit} ref="newsletterForm">
          {this.renderHiddenFormInputs()}
          <div>
            <input type="email" name="email" required="required" placeholder={content.scene2_email_placeholder_text} autoFocus={true} />
            <button type="submit" className="ASRouterButton primary" ref="formSubmitBtn">{content.scene2_button_label}</button>
          </div>
          {this.renderFormPrivacyNotice()}
        </form>
      </SnippetBase>);
  }

  render() {
    if (this.state.signupSubmitted) {
      return this.renderSignupSubmitted();
    }
    if (this.state.expanded) {
      return this.renderSignupView();
    }
    return <SimpleSnippet {...this.props} onButtonClick={this.expandSnippet} />;
  }
}
