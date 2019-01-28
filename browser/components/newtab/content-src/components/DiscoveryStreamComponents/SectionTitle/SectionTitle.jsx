import React from "react";

export class SectionTitle extends React.PureComponent {
  render() {
    const {header: {title, subtitle}} = this.props;
    return (
      <div className="ds-section-title">
        <div className="title">{title}</div>
        {subtitle ? <div className="subtitle">{subtitle}</div> : null}
      </div>
    );
  }
}
