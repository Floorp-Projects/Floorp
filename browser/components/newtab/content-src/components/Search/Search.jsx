/* globals ContentSearchUIController */
"use strict";

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {FormattedMessage, injectIntl} from "react-intl";
import {connect} from "react-redux";
import {IS_NEWTAB} from "content-src/lib/constants";
import React from "react";

export class _Search extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onSearchClick = this.onSearchClick.bind(this);
    this.onSearchHandoffClick = this.onSearchHandoffClick.bind(this);
    this.onSearchHandoffKeyDown = this.onSearchHandoffKeyDown.bind(this);
    this.onSearchHandoffPaste = this.onSearchHandoffPaste.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
    this.onSearchHandoffButtonMount = this.onSearchHandoffButtonMount.bind(this);
  }

  handleEvent(event) {
    // Also track search events with our own telemetry
    if (event.detail.type === "Search") {
      this.props.dispatch(ac.UserEvent({event: "SEARCH"}));
    }
  }

  onSearchClick(event) {
    window.gContentSearchController.search(event);
  }

  doSearchHandoff(text) {
    this.props.dispatch(ac.OnlyToMain({type: at.HANDOFF_SEARCH_TO_AWESOMEBAR, data: {text}}));
    this.props.dispatch(ac.UserEvent({event: "SEARCH_HANDOFF"}));
    if (text) {
      // We don't hide the in-content search if there is no text (user hit <Enter>)
      this.props.dispatch({type: at.HIDE_SEARCH});
    }
  }

  onSearchHandoffClick(event) {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked with the mouse, we
    // focus it. If the user types, transfer focus to awesomebar.
    // If the button is clicked from the keyboard, we focus the awesomebar normally.
    // This is to minimize confusion with users navigating with the keyboard and
    // users using assistive technologoy.
    event.preventDefault();
    const isKeyboardClick = event.clientX === 0 && event.clientY === 0;
    if (isKeyboardClick) {
      this.doSearchHandoff();
    } else {
      this._searchHandoffButton.focus();
    }
  }

  onSearchHandoffKeyDown(event) {
    if (event.key.length === 1 && !event.altKey && !event.ctrlKey && !event.metaKey) {
      // We only care about key strokes that will produce a character.
      this.doSearchHandoff(event.key);
    }
  }

  onSearchHandoffPaste(event) {
    if (!this._searchHandoffButton ||
        !this._searchHandoffButton.contains(global.document.activeElement)) {
      // Don't handle every paste on the document. Filter out those that are
      // not on the Search Hand-off button.
      return;
    }
    event.preventDefault();
    this.doSearchHandoff(event.clipboardData.getData("Text"));
  }

  componentWillMount() {
    if (global.document) {
      // We need to listen to paste events that bubble up from the Search Hand-off
      // button. Adding the paste listener to the button itself or it's parent
      // doesn't work consistently until the page is clicked on.
      global.document.addEventListener("paste", this.onSearchHandoffPaste);
    }
  }

  componentWillUnmount() {
    if (global.document) {
      global.document.removeEventListener("paste", this.onDocumentPaste);
    }
    delete window.gContentSearchController;
  }

  onInputMount(input) {
    if (input) {
      // The "healthReportKey" and needs to be "newtab" or "abouthome" so that
      // BrowserUsageTelemetry.jsm knows to handle events with this name, and
      // can add the appropriate telemetry probes for search. Without the correct
      // name, certain tests like browser_UsageTelemetry_content.js will fail
      // (See github ticket #2348 for more details)
      const healthReportKey = IS_NEWTAB ? "newtab" : "abouthome";

      // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)
      const searchSource = IS_NEWTAB ? "newtab" : "homepage";

      // gContentSearchController needs to exist as a global so that tests for
      // the existing about:home can find it; and so it allows these tests to pass.
      // In the future, when activity stream is default about:home, this can be renamed
      window.gContentSearchController = new ContentSearchUIController(input, input.parentNode,
        healthReportKey, searchSource);
      addEventListener("ContentSearchClient", this);
    } else {
      window.gContentSearchController = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  onSearchHandoffButtonMount(button) {
    // Keep a reference to the button for use during "paste" event handling.
    this._searchHandoffButton = button;
  }

  /*
   * Do not change the ID on the input field, as legacy newtab code
   * specifically looks for the id 'newtab-search-text' on input fields
   * in order to execute searches in various tests
   */
  render() {
    const wrapperClassName = [
      "search-wrapper",
      this.props.hide && "search-hidden",
    ].filter(v => v).join(" ");

    return (<div className={wrapperClassName}>
      {this.props.showLogo &&
        <div className="logo-and-wordmark">
          <div className="logo" />
          <div className="wordmark" />
        </div>
      }
      {!this.props.handoffEnabled &&
      <div className="search-inner-wrapper">
        <label htmlFor="newtab-search-text" className="search-label">
          <span className="sr-only"><FormattedMessage id="search_web_placeholder" /></span>
        </label>
        <input
          id="newtab-search-text"
          maxLength="256"
          placeholder={this.props.intl.formatMessage({id: "search_web_placeholder"})}
          ref={this.onInputMount}
          title={this.props.intl.formatMessage({id: "search_web_placeholder"})}
          type="search" />
        <button
          id="searchSubmit"
          className="search-button"
          onClick={this.onSearchClick}
          title={this.props.intl.formatMessage({id: "search_button"})}>
          <span className="sr-only"><FormattedMessage id="search_button" /></span>
        </button>
      </div>
      }
      {this.props.handoffEnabled &&
        <div className="search-inner-wrapper">
          <button
            className="search-handoff-button"
            ref={this.onSearchHandoffButtonMount}
            onClick={this.onSearchHandoffClick}
            onKeyDown={this.onSearchHandoffKeyDown}
            title={this.props.intl.formatMessage({id: "search_web_placeholder"})}>
            <div className="fake-textbox">{this.props.intl.formatMessage({id: "search_web_placeholder"})}</div>
            <div className="fake-editable" tabIndex="-1" aria-hidden="true" contentEditable="" />
            <div className="fake-caret" />
          </button>
          {/*
            This dummy and hidden input below is so we can load ContentSearchUIController.
            Why? It sets --newtab-search-icon for us and it isn't trivial to port over.
          */}
          <input
            type="search"
            style={{display: "none"}}
            ref={this.onInputMount} />
        </div>
      }
    </div>);
  }
}

export const Search = connect()(injectIntl(_Search));
