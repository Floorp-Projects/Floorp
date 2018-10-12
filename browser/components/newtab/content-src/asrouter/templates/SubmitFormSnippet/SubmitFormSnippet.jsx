import React from "react";
import {RichText} from "../../components/RichText/RichText";
import {SimpleSnippet} from "../SimpleSnippet/SimpleSnippet";
import {SnippetBase} from "../../components/SnippetBase/SnippetBase";

export class SubmitFormSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.expandSnippet = this.expandSnippet.bind(this);
    this.handleSubmit = this.handleSubmit.bind(this);
    this.state = {
      expanded: false,
      signupSubmitted: false,
      signupSuccess: false,
      disableForm: false,
    };
  }

  async handleSubmit(event) {
    let json;

    if (this.state.disableForm) {
      return;
    }

    event.preventDefault();
    this.setState({disableForm: true});
    this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", value: "conversion-subscribe-activation", id: "NEWTAB_FOOTER_BAR_CONTENT"});

    if (this.props.form_method.toUpperCase() === "GET") {
      this.refs.form.submit();
      return;
    }

    const fetchConfig = {
      body: new FormData(this.refs.form),
      method: "POST",
    };

    try {
      const fetchRequest = new Request(this.refs.form.action, fetchConfig);
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

    this.setState({disableForm: false});
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
    const {content} = this.props;
    if (!content.scene2_privacy_html) {
      return null;
    }
    return (<label className="privacy-notice" htmlFor="id_privacy">
        <p>
          <input type="checkbox" id="id_privacy" name="privacy" required="required" />
          <span><RichText text={content.scene2_privacy_html}
            localization_id="privacy_html"
            links={content.links}
            sendClick={this.props.sendClick} />
          </span>
        </p>
      </label>);
  }

  renderSignupSubmitted() {
    const message = this.state.signupSuccess ? this.props.content.success_text : this.props.content.error_text;
    const onButtonClick = !this.state.signupSuccess ? this.expandSnippet : null;

    return (<SimpleSnippet className={this.props.className}
      onButtonClick={onButtonClick}
      provider={this.props.provider}
      content={{button_label: this.props.content.scene1_button_label, text: message}} />);
  }

  renderSignupView() {
    const {content} = this.props;

    return (<SnippetBase {...this.props} className="SubmitFormSnippet" footerDismiss={true}>
        <div className="message">
          <p>{content.scene2_text}</p>
        </div>
        <form action={content.form_action} method={this.props.form_method} onSubmit={this.handleSubmit} ref="form">
          {this.renderHiddenFormInputs()}
          <div>
            <input type="email" name="email" required="required" placeholder={content.scene2_email_placeholder_text} autoFocus={true} />
            <button type="submit" className="ASRouterButton primary" ref="formSubmitBtn">{content.scene2_button_label}</button>
          </div>
          {this.renderFormPrivacyNotice()}
        </form>
      </SnippetBase>);
  }

  getFirstSceneContent() {
    return Object.keys(this.props.content).filter(key => key.includes("scene1")).reduce((acc, key) => {
      acc[key.substr(7)] = this.props.content[key];
      return acc;
    }, {});
  }

  render() {
    const content = {...this.props.content, ...this.getFirstSceneContent()};

    if (this.state.signupSubmitted) {
      return this.renderSignupSubmitted();
    }
    if (this.state.expanded) {
      return this.renderSignupView();
    }
    return <SimpleSnippet {...this.props} content={content} onButtonClick={this.expandSnippet} />;
  }
}
