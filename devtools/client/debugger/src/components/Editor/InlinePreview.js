/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";
import Reps from "devtools/client/shared/components/reps/index";

const {
  REPS: {
    Rep,
    ElementNode: { supportsObject: isElement },
  },
  MODE,
} = Reps;

// Renders single variable preview inside a codemirror line widget
class InlinePreview extends PureComponent {
  static get propTypes() {
    return {
      highlightDomElement: PropTypes.func.isRequired,
      openElementInInspector: PropTypes.func.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
      value: PropTypes.any,
      variable: PropTypes.string.isRequired,
    };
  }

  showInScopes(variable) {
    // TODO: focus on variable value in the scopes sidepanel
    // we will need more info from parent comp
  }

  render() {
    const {
      value,
      variable,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
    } = this.props;

    const mode = isElement(value) ? MODE.TINY : MODE.SHORT;

    return (
      <span
        className="inline-preview-outer"
        onClick={() => this.showInScopes(variable)}
      >
        <span className="inline-preview-label">{variable}:</span>
        <span className="inline-preview-value">
          <Rep
            object={value}
            mode={mode}
            onDOMNodeClick={grip => openElementInInspector(grip)}
            onInspectIconClick={grip => openElementInInspector(grip)}
            onDOMNodeMouseOver={grip => highlightDomElement(grip)}
            onDOMNodeMouseOut={grip => unHighlightDomElement(grip)}
          />
        </span>
      </span>
    );
  }
}

export default InlinePreview;
