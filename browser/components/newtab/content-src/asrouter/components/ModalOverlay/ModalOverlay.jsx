import React from "react";

export class ModalOverlayWrapper extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  onKeyDown(event) {
    if (event.key === "Escape") {
      this.props.onClose();
    }
  }

  componentWillMount() {
    this.props.document.addEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.add("modal-open");
  }

  componentWillUnmount() {
    this.props.document.removeEventListener("keydown", this.onKeyDown);
    this.props.document.body.classList.remove("modal-open");
  }

  render() {
    const {props} = this;
    return (<React.Fragment>
      <div className="modalOverlayOuter active" onClick={props.onClose} role="presentation" />
      <div className={`modalOverlayInner active ${props.innerClassName || ""}`}>
        {props.children}
      </div>
    </React.Fragment>);
  }
}

ModalOverlayWrapper.defaultProps = {document: global.document};

export class ModalOverlay extends React.PureComponent {
  render() {
    const {title, button_label} = this.props;
    return (
      <ModalOverlayWrapper onClose={this.props.onDoneButton}>
        <h2> {title} </h2>
        {this.props.children}
        <div className="footer">
          <button className="button primary modalButton"
            onClick={this.props.onDoneButton}> {button_label} </button>
        </div>
      </ModalOverlayWrapper>);
  }
}
