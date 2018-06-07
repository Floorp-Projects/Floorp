import React from "react";

export class ModalOverlay extends React.PureComponent {
  componentWillMount() {
    this.setState({active: true});
    document.body.classList.add("modal-open");
  }

  componentWillUnmount() {
    document.body.classList.remove("modal-open");
    this.setState({active: false});
  }

  render() {
    const {active} = this.state;
    const {title, button_label} = this.props;
    return (
      <div>
        <div className={`modalOverlayOuter ${active ? "active" : ""}`} />
        <div className={`modalOverlayInner ${active ? "active" : ""}`}>
          <h2> {title} </h2>
          {this.props.children}
          <div className="footer">
            <button onClick={this.props.onDoneButton} className="button primary modalButton"> {button_label} </button>
          </div>
        </div>
      </div>
    );
  }
}
