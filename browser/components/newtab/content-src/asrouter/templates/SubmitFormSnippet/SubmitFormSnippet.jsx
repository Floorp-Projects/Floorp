import {Button} from "../../components/Button/Button";
import React from "react";
import {RichText} from "../../components/RichText/RichText";
import {SimpleSnippet} from "../SimpleSnippet/SimpleSnippet";
import {SnippetBase} from "../../components/SnippetBase/SnippetBase";

export class SubmitFormSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.expandSnippet = this.expandSnippet.bind(this);
    this.handleSubmit = this.handleSubmit.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
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

    const {url, formData} = this.props.processFormData ?
      this.props.processFormData(this.refs.mainInput, this.props) :
      {url: this.refs.form.action, formData: new FormData(this.refs.form)};

    try {
      const fetchRequest = new Request(url, {body: formData, method: "POST"});
      const response = await fetch(fetchRequest);
      json = await response.json();
    } catch (err) {
      console.log(err); // eslint-disable-line no-console
    }

    if (json && json.status === "ok") {
      this.setState({signupSuccess: true, signupSubmitted: true});
      if (!this.props.content.do_not_autoblock) {
        this.props.onBlock({preventDismiss: true});
      }
      this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", value: "subscribe-success", id: "NEWTAB_FOOTER_BAR_CONTENT"});
    } else {
      console.error("There was a problem submitting the form", json || "[No JSON response]"); // eslint-disable-line no-console
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

  renderDisclaimer() {
    const {content} = this.props;
    if (!content.scene2_disclaimer_html) {
      return null;
    }
    return (<p className="disclaimerText">
      <RichText text={content.scene2_disclaimer_html}
        localization_id="disclaimer_html"
        links={content.links}
        doNotAutoBlock={true}
        sendClick={this.props.sendClick} />
    </p>);
  }

  renderFormPrivacyNotice() {
    const {content} = this.props;
    if (!content.scene2_privacy_html) {
      return null;
    }
    return (<label className="privacyNotice" htmlFor="id_privacy">
        <p>
          <input type="checkbox" id="id_privacy" name="privacy" required="required" />
          <RichText text={content.scene2_privacy_html}
            localization_id="privacy_html"
            links={content.links}
            doNotAutoBlock={true}
            sendClick={this.props.sendClick} />
        </p>
      </label>);
  }

  renderSignupSubmitted() {
    const {content} = this.props;
    const isSuccess = this.state.signupSuccess;
    const successTitle = isSuccess && content.success_title;
    const bodyText = isSuccess ? content.success_text : content.error_text;
    const retryButtonText = content.scene1_button_label;
    return (<SnippetBase {...this.props}><div className="submissionStatus">
      {successTitle ? <h2 className="submitStatusTitle">{successTitle}</h2> : null}
      <p>{bodyText}{isSuccess ? null : <Button onClick={this.expandSnippet}>{retryButtonText}</Button>}</p>
    </div></SnippetBase>);
  }

  onInputChange(event) {
    if (!this.props.validateInput) {
      return;
    }
    const hasError = this.props.validateInput(event.target.value, this.props.content);
    event.target.setCustomValidity(hasError);
  }

  renderInput() {
    const placholder = this.props.content.scene2_email_placeholder_text || this.props.content.scene2_input_placeholder;
    return (<input
      ref="mainInput"
      type={this.props.inputType || "email"}
      className="mainInput"
      name="email"
      required={true}
      placeholder={placholder}
      onChange={this.props.validateInput ? this.onInputChange : null}
      autoFocus={true} />);
  }

  renderSignupView() {
    const {content} = this.props;
    const containerClass = `SubmitFormSnippet ${this.props.className}`;
    return (<SnippetBase {...this.props} className={containerClass} footerDismiss={true}>
        {content.scene2_icon ? <div className="scene2Icon"><img src={content.scene2_icon} /></div> : null}
        <div className="message">
          <p>
            {content.scene2_title ? <h3 className="scene2Title">{content.scene2_title}</h3> : null}
            {" "}
            <RichText scene2_text={content.scene2_text} localization_id="scene2_text" />
          </p>
        </div>
        <form action={content.form_action} method={this.props.form_method} onSubmit={this.handleSubmit} ref="form">
          {this.renderHiddenFormInputs()}
          <div>
            {this.renderInput()}
            <button type="submit" className="ASRouterButton primary" ref="formSubmitBtn">{content.scene2_button_label}</button>
          </div>
          {this.renderFormPrivacyNotice() || this.renderDisclaimer()}
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
