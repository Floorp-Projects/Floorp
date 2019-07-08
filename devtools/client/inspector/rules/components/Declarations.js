/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Declaration = createFactory(require("./Declaration"));

const Types = require("../types");

class Declarations extends PureComponent {
  static get propTypes() {
    return {
      declarations: PropTypes.arrayOf(PropTypes.shape(Types.declaration))
        .isRequired,
      isUserAgentStyle: PropTypes.bool.isRequired,
      onToggleDeclaration: PropTypes.func.isRequired,
      showDeclarationNameEditor: PropTypes.func.isRequired,
      showDeclarationValueEditor: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      declarations,
      isUserAgentStyle,
      onToggleDeclaration,
      showDeclarationNameEditor,
      showDeclarationValueEditor,
    } = this.props;

    if (!declarations.length) {
      return null;
    }

    return dom.ul(
      { className: "ruleview-propertylist" },
      declarations.map(declaration => {
        if (declaration.isInvisible) {
          return null;
        }

        return Declaration({
          key: declaration.id,
          declaration,
          isUserAgentStyle,
          onToggleDeclaration,
          showDeclarationNameEditor,
          showDeclarationValueEditor,
        });
      })
    );
  }
}

module.exports = Declarations;
