import React from "react";

export class SafeAnchor extends React.PureComponent {
  safeURI(url) {
    let protocol = null;
    try {
      protocol = new URL(url).protocol;
    } catch (e) { return ""; }

    const isAllowed = [
      "http:",
      "https:",
    ].includes(protocol);
    if (!isAllowed) {
      console.warn(`${protocol} is not allowed for anchor targets.`); // eslint-disable-line no-console
      return "";
    }
    return url;
  }

  render() {
    const {url, className, onLinkClick} = this.props;
    return (
      <a href={this.safeURI(url)} className={className} onClick={onLinkClick}>
        {this.props.children}
      </a>
    );
  }
}
