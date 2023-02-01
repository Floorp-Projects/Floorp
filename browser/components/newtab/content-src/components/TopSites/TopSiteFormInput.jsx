/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class TopSiteFormInput extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { validationError: this.props.validationError };
    this.onChange = this.onChange.bind(this);
    this.onMount = this.onMount.bind(this);
    this.onClearIconPress = this.onClearIconPress.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (nextProps.shouldFocus && !this.props.shouldFocus) {
      this.input.focus();
    }
    if (nextProps.validationError && !this.props.validationError) {
      this.setState({ validationError: true });
    }
    // If the component is in an error state but the value was cleared by the parent
    if (this.state.validationError && !nextProps.value) {
      this.setState({ validationError: false });
    }
  }

  onClearIconPress(event) {
    // If there is input in the URL or custom image URL fields,
    // and we hit 'enter' while tabbed over the clear icon,
    // we should execute the function to clear the field.
    if (event.key === "Enter") {
      this.props.onClear();
    }
  }

  onChange(ev) {
    if (this.state.validationError) {
      this.setState({ validationError: false });
    }
    this.props.onChange(ev);
  }

  onMount(input) {
    this.input = input;
  }

  renderLoadingOrCloseButton() {
    const showClearButton = this.props.value && this.props.onClear;

    if (this.props.loading) {
      return (
        <div className="loading-container">
          <div className="loading-animation" />
        </div>
      );
    } else if (showClearButton) {
      return (
        <button
          type="button"
          className="icon icon-clear-input icon-button-style"
          onClick={this.props.onClear}
          onKeyPress={this.onClearIconPress}
        />
      );
    }
    return null;
  }

  render() {
    const { typeUrl } = this.props;
    const { validationError } = this.state;

    return (
      <label>
        <span data-l10n-id={this.props.titleId} />
        <div
          className={`field ${typeUrl ? "url" : ""}${
            validationError ? " invalid" : ""
          }`}
        >
          <input
            type="text"
            value={this.props.value}
            ref={this.onMount}
            onChange={this.onChange}
            data-l10n-id={this.props.placeholderId}
            // Set focus on error if the url field is valid or when the input is first rendered and is empty
            // eslint-disable-next-line jsx-a11y/no-autofocus
            autoFocus={this.props.autoFocusOnOpen}
            disabled={this.props.loading}
          />
          {this.renderLoadingOrCloseButton()}
          {validationError && (
            <aside
              className="error-tooltip"
              data-l10n-id={this.props.errorMessageId}
            />
          )}
        </div>
      </label>
    );
  }
}

TopSiteFormInput.defaultProps = {
  showClearButton: false,
  value: "",
  validationError: false,
};
