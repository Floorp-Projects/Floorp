import React from "react";

export class DSCard extends React.PureComponent {
  render() {
    return (
      <div className="ds-card">
        <img src={this.props.image_src} />
        <div className="meta">
          <header>{this.props.title}</header>
          <p>{this.props.excerpt}</p>
          <p>{this.props.source}</p>
        </div>
      </div>
    );
  }
}
