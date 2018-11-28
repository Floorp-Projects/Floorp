import React from "react";
import schema from "../../templates/SimpleSnippet/SimpleSnippet.schema.json";

export class SnippetBase extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
  }

  onBlockClicked() {
    if (this.props.provider !== "preview") {
      this.props.sendUserActionTelemetry({event: "BLOCK", id: this.props.UISurface});
    }

    this.props.onBlock();
  }

  renderDismissButton() {
    if (this.props.footerDismiss) {
      return (
        <div className="footer">
          <div className="footer-content">
            <button
              className="ASRouterButton secondary"
              onClick={this.props.onDismiss}>
              {this.props.content.scene2_dismiss_button_text}
            </button>
          </div>
        </div>
      );
    }

    const defaultTitle = schema.properties.block_button_text.default;
    return (
      <button className="blockButton" title={this.props.content.block_button_text || defaultTitle} onClick={this.onBlockClicked} />
    );
  }

  render() {
    const {props} = this;

    const containerClassName = `SnippetBaseContainer${props.className ? ` ${props.className}` : ""}`;

    return (<div className={containerClassName} style={this.props.textStyle}>
      <div className="innerWrapper">
        {props.children}
      </div>
      {this.renderDismissButton()}
    </div>);
  }
}
