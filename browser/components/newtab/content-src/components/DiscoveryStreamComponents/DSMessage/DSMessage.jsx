import React from "react";
import {SafeAnchor} from "../SafeAnchor/SafeAnchor";

export class DSMessage extends React.PureComponent {
  render() {
    return (
      <div className="ds-message">
        <header className="title">
          {this.props.icon && (<div className="glyph" style={{backgroundImage: `url(${this.props.icon})`}} />)}
          {this.props.title && (<span className="title-text">{this.props.title}</span>)}
          {this.props.link_text && this.props.link_url && (<SafeAnchor className="link" url={this.props.link_url}>{this.props.link_text}</SafeAnchor>)}
        </header>
      </div>
    );
  }
}
