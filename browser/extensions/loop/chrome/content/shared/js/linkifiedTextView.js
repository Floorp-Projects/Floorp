var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = loop.shared.views || {};
loop.shared.views.LinkifiedTextView = function () {
  "use strict";

  /**
   * Given a rawText property, renderer a version of that text with any
   * links starting with http://, https://, or ftp:// as actual clickable
   * links inside a <p> container.
   */

  var LinkifiedTextView = React.createClass({
    displayName: "LinkifiedTextView",

    propTypes: {
      // Call this instead of allowing the default <a> click semantics, if
      // given.  Also causes sendReferrer and suppressTarget attributes to be
      // ignored.
      linkClickHandler: React.PropTypes.func,
      // The text to be linkified.
      rawText: React.PropTypes.string.isRequired,
      // Should the links send a referrer?  Defaults to false.
      sendReferrer: React.PropTypes.bool,
      // Should we suppress target="_blank" on the link? Defaults to false.
      // Mostly for testing use.
      suppressTarget: React.PropTypes.bool
    },

    mixins: [React.addons.PureRenderMixin],

    _handleClickEvent: function (e) {
      e.preventDefault();
      e.stopPropagation();

      this.props.linkClickHandler(e.currentTarget.href);
    },

    _generateLinkAttributes: function (href) {
      var linkAttributes = {
        href: href
      };

      if (this.props.linkClickHandler) {
        linkAttributes.onClick = this._handleClickEvent;

        // if this is specified, we short-circuit return to avoid unnecessarily
        // creating target and rel attributes.
        return linkAttributes;
      }

      if (!this.props.suppressTarget) {
        linkAttributes.target = "_blank";
      }

      if (!this.props.sendReferrer) {
        linkAttributes.rel = "noreferrer";
      }

      return linkAttributes;
    },

    /**                                                              a
     * Parse the given string into an array of strings and React <a> elements
     * in the order in which they should be rendered (i.e. FIFO).
     *
     * @param {String} s the raw string to be parsed
     *
     * @returns {Array} of strings and React <a> elements in order.
     */
    parseStringToElements: function (s) {
      var elements = [];
      var result = loop.shared.urlRegExps.fullUrlMatch.exec(s);
      var reactElementsCounter = 0; // For giving keys to each ReactElement.

      while (result) {
        // If there's text preceding the first link, push it onto the array
        // and update the string pointer.
        if (result.index) {
          elements.push(s.substr(0, result.index));
          s = s.substr(result.index);
        }

        // Push the first link itself, and advance the string pointer again.
        elements.push(React.createElement(
          "a",
          _extends({}, this._generateLinkAttributes(result[0]), {
            key: reactElementsCounter++ }),
          result[0]
        ));
        s = s.substr(result[0].length);

        // Check for another link, and perhaps continue...
        result = loop.shared.urlRegExps.fullUrlMatch.exec(s);
      }

      if (s) {
        elements.push(s);
      }

      return elements;
    },

    render: function () {
      return React.createElement(
        "p",
        null,
        this.parseStringToElements(this.props.rawText)
      );
    }
  });

  return LinkifiedTextView;
}();
