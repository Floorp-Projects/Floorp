import React from "react";

export function A11yLinkButton(props) {
  // function for merging classes, if necessary
  let className = "a11y-link-button";
  if (props.className) {
    className += ` ${props.className}`;
  }
  return (
    <button type="button" {...props} className={className}>
      {props.children}
    </button>
  );
}
