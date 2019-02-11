import React from "react";

export class DSMessage extends React.PureComponent {
  render() {
    let hasSubtitleAndOrLink = this.props.link_text && this.props.link_url;
    hasSubtitleAndOrLink = hasSubtitleAndOrLink || this.props.subtitle;

    return (
      <div className="ds-message">
        {this.props.title && (
          <header className="title">
            {this.props.icon && (<img src={this.props.icon} />)}
            <span>{this.props.title}</span>
          </header>
        )}
        { hasSubtitleAndOrLink && (
          <p className="subtitle">
            {this.props.subtitle && (<span>{this.props.subtitle}</span>)}
            {this.props.link_text && this.props.link_url && (<a href={this.props.link_url}>{this.props.link_text}</a>)}
          </p>
        )}
        <hr className="ds-hr" />
      </div>
    );
  }
}
