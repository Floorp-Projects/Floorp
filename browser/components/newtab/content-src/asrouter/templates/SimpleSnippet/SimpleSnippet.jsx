import {Button} from "../../components/Button/Button";
import {ConditionalWrapper} from "../../components/ConditionalWrapper/ConditionalWrapper";
import React from "react";
import {RichText} from "../../components/RichText/RichText";
import {safeURI} from "../../template-utils";
import {SnippetBase} from "../../components/SnippetBase/SnippetBase";

const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png";
// Alt text if available; in the future this should come from the server. See bug 1551711
const ICON_ALT_TEXT = "";

export class SimpleSnippet extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onButtonClick = this.onButtonClick.bind(this);
  }

  onButtonClick() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({event: "CLICK_BUTTON", id: this.props.UISurface});
    }
    const {button_url} = this.props.content;
    // If button_url is defined handle it as OPEN_URL action
    const type = this.props.content.button_action || (button_url && "OPEN_URL");
    this.props.onAction({
      type,
      data: {args: this.props.content.button_action_args || button_url},
    });
    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
  }

  _shouldRenderButton() {
    return this.props.content.button_action || this.props.onButtonClick || this.props.content.button_url;
  }

  renderTitle() {
    const {title} = this.props.content;
    return title ?
      <h3 className={`title ${this._shouldRenderButton() ? "title-inline" : ""}`}>{this.renderTitleIcon()} {title}</h3> :
      null;
  }

  renderTitleIcon() {
    const titleIconLight = safeURI(this.props.content.title_icon);
    const titleIconDark = safeURI(this.props.content.title_icon_dark_theme || this.props.content.title_icon);
    if (!titleIconLight) {
      return null;
    }

    return (<React.Fragment>
        <span className="titleIcon icon-light-theme" style={{backgroundImage: `url("${titleIconLight}")`}} />
        <span className="titleIcon icon-dark-theme" style={{backgroundImage: `url("${titleIconDark}")`}} />
      </React.Fragment>);
  }

  renderButton() {
    const {props} = this;
    if (!this._shouldRenderButton()) {
      return null;
    }

    return (<Button
      onClick={props.onButtonClick || this.onButtonClick}
      color={props.content.button_color}
      backgroundColor={props.content.button_background_color}>
      {props.content.button_label}
    </Button>);
  }

  renderText() {
    const {props} = this;
    return (<RichText text={props.content.text}
      customElements={this.props.customElements}
      localization_id="text"
      links={props.content.links}
      sendClick={props.sendClick} />);
  }

  wrapSectionHeader(url) {
    return function(children) {
      return <a href={url}>{children}</a>;
    };
  }

  wrapSnippetContent(children) {
    return <div className="innerContentWrapper">{children}</div>;
  }

  renderSectionHeader() {
    const {props} = this;

    // an icon and text must be specified to render the section header
    if (props.content.section_title_icon && props.content.section_title_text) {
      const sectionTitleIconLight = safeURI(props.content.section_title_icon);
      const sectionTitleIconDark = safeURI(props.content.section_title_icon_dark_theme || props.content.section_title_icon);
      const sectionTitleURL = props.content.section_title_url;

      return (
        <div className="section-header">
          <h3 className="section-title">
            <ConditionalWrapper condition={sectionTitleURL} wrap={this.wrapSectionHeader(sectionTitleURL)}>
              <span className="icon icon-small-spacer icon-light-theme" style={{backgroundImage: `url("${sectionTitleIconLight}")`}} />
              <span className="icon icon-small-spacer icon-dark-theme" style={{backgroundImage: `url("${sectionTitleIconDark}")`}} />
              <span className="section-title-text">{props.content.section_title_text}</span>
            </ConditionalWrapper>
          </h3>
        </div>
      );
    }

    return null;
  }

  render() {
    const {props} = this;
    const sectionHeader = this.renderSectionHeader();
    let className = "SimpleSnippet";

    if (props.className) {
      className += ` ${props.className}`;
    }
    if (props.content.tall) {
      className += " tall";
    }
    if (sectionHeader) {
      className += " has-section-header";
    }

    return (<SnippetBase {...props} className={className} textStyle={this.props.textStyle}>
      {sectionHeader}
      <ConditionalWrapper condition={sectionHeader} wrap={this.wrapSnippetContent}>
        <img src={safeURI(props.content.icon) || DEFAULT_ICON_PATH} className="icon icon-light-theme" alt={ICON_ALT_TEXT} />
        <img src={safeURI(props.content.icon_dark_theme || props.content.icon) || DEFAULT_ICON_PATH} className="icon icon-dark-theme" alt={ICON_ALT_TEXT} />
        <div>
          {this.renderTitle()} <p className="body">{this.renderText()}</p>
          {this.props.extraContent}
        </div>
        {<div>{this.renderButton()}</div>}
      </ConditionalWrapper>
    </SnippetBase>);
  }
}
