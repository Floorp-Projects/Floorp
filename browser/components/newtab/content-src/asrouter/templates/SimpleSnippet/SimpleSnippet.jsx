import {Button} from "../../components/Button/Button";
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

  renderTitle() {
    const {title} = this.props.content;
    return title ? <h3 className="title">{title}</h3> : null;
  }

  renderTitleIcon() {
    const titleIcon = safeURI(this.props.content.title_icon);
    return titleIcon ? <span className="titleIcon" style={{backgroundImage: `url("${titleIcon}")`}} /> : null;
  }

  renderButton() {
    const {props} = this;
    if (!props.content.button_action && !props.onButtonClick && !props.content.button_url) {
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
      localization_id="text"
      links={props.content.links}
      sendClick={props.sendClick} />);
  }

  render() {
    const {props} = this;
    const className = `SimpleSnippet${props.content.tall ? " tall" : ""}`;
    return (<SnippetBase {...props} className={className}>
      <img src={safeURI(props.content.icon) || DEFAULT_ICON_PATH} className="icon" />
      <div>
        {this.renderTitleIcon()} {this.renderTitle()} <p className="body">{this.renderText()}</p>
      </div>
      {<div>{this.renderButton()}</div>}
    </SnippetBase>);
  }
}
