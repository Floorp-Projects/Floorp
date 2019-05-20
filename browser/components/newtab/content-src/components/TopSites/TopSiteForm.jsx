import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {A11yLinkButton} from "content-src/components/A11yLinkButton/A11yLinkButton";
import {FormattedMessage} from "react-intl";
import React from "react";
import {TOP_SITES_SOURCE} from "./TopSitesConstants";
import {TopSiteFormInput} from "./TopSiteFormInput";
import {TopSiteLink} from "./TopSite";

export class TopSiteForm extends React.PureComponent {
  constructor(props) {
    super(props);
    const {site} = props;
    this.state = {
      label: site ? (site.label || site.hostname) : "",
      url: site ? site.url : "",
      validationError: false,
      customScreenshotUrl: site ? site.customScreenshotURL : "",
      showCustomScreenshotForm: site ? site.customScreenshotURL : false,
    };
    this.onClearScreenshotInput = this.onClearScreenshotInput.bind(this);
    this.onLabelChange = this.onLabelChange.bind(this);
    this.onUrlChange = this.onUrlChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onClearUrlClick = this.onClearUrlClick.bind(this);
    this.onDoneButtonClick = this.onDoneButtonClick.bind(this);
    this.onCustomScreenshotUrlChange = this.onCustomScreenshotUrlChange.bind(this);
    this.onPreviewButtonClick = this.onPreviewButtonClick.bind(this);
    this.onEnableScreenshotUrlForm = this.onEnableScreenshotUrlForm.bind(this);
    this.validateUrl = this.validateUrl.bind(this);
  }

  onLabelChange(event) {
    this.setState({"label": event.target.value});
  }

  onUrlChange(event) {
    this.setState({
      url: event.target.value,
      validationError: false,
    });
  }

  onClearUrlClick() {
    this.setState({
      url: "",
      validationError: false,
    });
  }

  onEnableScreenshotUrlForm() {
    this.setState({showCustomScreenshotForm: true});
  }

  _updateCustomScreenshotInput(customScreenshotUrl) {
    this.setState({
      customScreenshotUrl,
      validationError: false,
    });
    this.props.dispatch({type: at.PREVIEW_REQUEST_CANCEL});
  }

  onCustomScreenshotUrlChange(event) {
    this._updateCustomScreenshotInput(event.target.value);
  }

  onClearScreenshotInput() {
    this._updateCustomScreenshotInput("");
  }

  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }

  onDoneButtonClick(ev) {
    ev.preventDefault();

    if (this.validateForm()) {
      const site = {url: this.cleanUrl(this.state.url)};
      const {index} = this.props;
      if (this.state.label !== "") {
        site.label = this.state.label;
      }

      if (this.state.customScreenshotUrl) {
        site.customScreenshotURL = this.cleanUrl(this.state.customScreenshotUrl);
      } else if (this.props.site && this.props.site.customScreenshotURL) {
        // Used to flag that previously cached screenshot should be removed
        site.customScreenshotURL = null;
      }
      this.props.dispatch(ac.AlsoToMain({
        type: at.TOP_SITES_PIN,
        data: {site, index},
      }));
      this.props.dispatch(ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "TOP_SITES_EDIT",
        action_position: index,
      }));

      this.props.onClose();
    }
  }

  onPreviewButtonClick(event) {
    event.preventDefault();
    if (this.validateForm()) {
      this.props.dispatch(ac.AlsoToMain({
        type: at.PREVIEW_REQUEST,
        data: {url: this.cleanUrl(this.state.customScreenshotUrl)},
      }));
      this.props.dispatch(ac.UserEvent({
        source: TOP_SITES_SOURCE,
        event: "PREVIEW_REQUEST",
      }));
    }
  }

  cleanUrl(url) {
    // If we are missing a protocol, prepend http://
    if (!url.startsWith("http:") && !url.startsWith("https:")) {
      return `http://${url}`;
    }
    return url;
  }

  _tryParseUrl(url) {
    try {
      return new URL(url);
    } catch (e) {
      return null;
    }
  }

  validateUrl(url) {
    const validProtocols = ["http:", "https:"];
    const urlObj = this._tryParseUrl(url) || this._tryParseUrl(this.cleanUrl(url));

    return urlObj && validProtocols.includes(urlObj.protocol);
  }

  validateCustomScreenshotUrl() {
    const {customScreenshotUrl} = this.state;
    return !customScreenshotUrl || this.validateUrl(customScreenshotUrl);
  }

  validateForm() {
    const validate = this.validateUrl(this.state.url) && this.validateCustomScreenshotUrl();

    if (!validate) {
      this.setState({validationError: true});
    }

    return validate;
  }

  _renderCustomScreenshotInput() {
    const {customScreenshotUrl} = this.state;
    const requestFailed = this.props.previewResponse === "";
    const validationError = (this.state.validationError && !this.validateCustomScreenshotUrl()) || requestFailed;
    // Set focus on error if the url field is valid or when the input is first rendered and is empty
    const shouldFocus = (validationError && this.validateUrl(this.state.url)) || !customScreenshotUrl;
    const isLoading = this.props.previewResponse === null &&
      customScreenshotUrl && this.props.previewUrl === this.cleanUrl(customScreenshotUrl);

    if (!this.state.showCustomScreenshotForm) {
      return (<A11yLinkButton onClick={this.onEnableScreenshotUrlForm} className="enable-custom-image-input">
                <FormattedMessage id="topsites_form_use_image_link" />
              </A11yLinkButton>);
    }
    return (<div className="custom-image-input-container">
      <TopSiteFormInput
        errorMessageId={requestFailed ? "topsites_form_image_validation" : "topsites_form_url_validation"}
        loading={isLoading}
        onChange={this.onCustomScreenshotUrlChange}
        onClear={this.onClearScreenshotInput}
        shouldFocus={shouldFocus}
        typeUrl={true}
        value={customScreenshotUrl}
        validationError={validationError}
        titleId="topsites_form_image_url_label"
        placeholderId="topsites_form_url_placeholder"
        intl={this.props.intl} />
    </div>);
  }

  render() {
    const {customScreenshotUrl} = this.state;
    const requestFailed = this.props.previewResponse === "";
    // For UI purposes, editing without an existing link is "add"
    const showAsAdd = !this.props.site;
    const previous = (this.props.site && this.props.site.customScreenshotURL) || "";
    const changed = customScreenshotUrl && this.cleanUrl(customScreenshotUrl) !== previous;
    // Preview mode if changes were made to the custom screenshot URL and no preview was received yet
    // or the request failed
    const previewMode = changed && !this.props.previewResponse;
    const previewLink = Object.assign({}, this.props.site);
    if (this.props.previewResponse) {
      previewLink.screenshot = this.props.previewResponse;
      previewLink.customScreenshotURL = this.props.previewUrl;
    }
    // Handles the form submit so an enter press performs the correct action
    const onSubmit = previewMode ? this.onPreviewButtonClick : this.onDoneButtonClick;
    return (
      <form className="topsite-form" onSubmit={onSubmit}>
        <div className="form-input-container">
          <h3 className="section-title">
            <FormattedMessage id={showAsAdd ? "topsites_form_add_header" : "topsites_form_edit_header"} />
          </h3>
          <div className="fields-and-preview">
            <div className="form-wrapper">
              <TopSiteFormInput onChange={this.onLabelChange}
                value={this.state.label}
                titleId="topsites_form_title_label"
                placeholderId="topsites_form_title_placeholder"
                intl={this.props.intl} />
              <TopSiteFormInput onChange={this.onUrlChange}
                shouldFocus={this.state.validationError && !this.validateUrl(this.state.url)}
                value={this.state.url}
                onClear={this.onClearUrlClick}
                validationError={this.state.validationError && !this.validateUrl(this.state.url)}
                titleId="topsites_form_url_label"
                typeUrl={true}
                placeholderId="topsites_form_url_placeholder"
                errorMessageId="topsites_form_url_validation"
                intl={this.props.intl} />
              {this._renderCustomScreenshotInput()}
            </div>
            <TopSiteLink link={previewLink}
              defaultStyle={requestFailed}
              title={this.state.label} />
          </div>
        </div>
        <section className="actions">
          <button className="cancel" type="button" onClick={this.onCancelButtonClick} >
            <FormattedMessage id="topsites_form_cancel_button" />
          </button>
          {previewMode ?
            <button className="done preview" type="submit" >
              <FormattedMessage id="topsites_form_preview_button" />
            </button> :
            <button className="done" type="submit" >
              <FormattedMessage id={showAsAdd ? "topsites_form_add_button" : "topsites_form_save_button"} />
            </button>}
        </section>
      </form>
    );
  }
}

TopSiteForm.defaultProps = {
  site: null,
  index: -1,
};
