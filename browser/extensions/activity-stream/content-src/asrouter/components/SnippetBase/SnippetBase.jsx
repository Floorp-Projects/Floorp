import React from "react";

export class SnippetBase extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onBlockClicked = this.onBlockClicked.bind(this);
  }

  onBlockClicked() {
    this.props.sendUserActionTelemetry({event: "BLOCK", id: this.props.UISurface});
    this.props.onBlock();
  }

  render() {
    const {props} = this;

    const containerClassName = `SnippetBaseContainer${props.className ? ` ${props.className}` : ""}`;

    return (<div className={containerClassName}>
      <div className="innerWrapper">
        {props.children}
      </div>
      <button className="blockButton" onClick={this.onBlockClicked} />
    </div>);
  }
}
