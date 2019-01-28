import {Button} from "../../components/Button/Button";
import {ConditionalWrapper} from "../../components/ConditionalWrapper/ConditionalWrapper";
import React from "react";
import {RichText} from "../../components/RichText/RichText";
import {safeURI} from "../../template-utils";
import {SnippetBase} from "../../components/SnippetBase/SnippetBase";

const DEFAULT_ICON_PATH = "chrome://branding/content/icon64.png";

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
    const titleIcon = safeURI(this.props.content.title_icon);
    return titleIcon ? <span className="titleIcon" style={{backgroundImage: `url("${titleIcon}")`}} /> : null;
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
      const sectionTitleIcon = safeURI(props.content.section_title_icon);
      const sectionTitleURL = props.content.section_title_url;

      return (
        <div className="section-header">
          <h3 className="section-title">
            <ConditionalWrapper condition={sectionTitleURL} wrap={this.wrapSectionHeader(sectionTitleURL)}>
              <span className="icon icon-small-spacer" style={{backgroundImage: `url("${sectionTitleIcon}")`}} />
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
        <img src={safeURI(props.content.icon) || DEFAULT_ICON_PATH} className="icon" />
        <div>
          {this.renderTitle()} <p className="body">{this.renderText()}</p>
          {this.props.extraContent}
        </div>
        {<div>{this.renderButton()}</div>}
      </ConditionalWrapper>
    </SnippetBase>);
  }
}
