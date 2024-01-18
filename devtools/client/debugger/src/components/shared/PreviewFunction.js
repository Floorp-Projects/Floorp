/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "devtools/client/shared/vendor/react";
import {
  span,
  button,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { formatDisplayName } from "../../utils/pause/frames/index";

const IGNORED_SOURCE_URLS = ["debugger eval code"];

export default class PreviewFunction extends Component {
  static get propTypes() {
    return {
      func: PropTypes.object.isRequired,
    };
  }

  renderFunctionName(func) {
    const { l10n } = this.context;
    const name = formatDisplayName(func, undefined, l10n);
    return span(
      {
        className: "function-name",
      },
      name
    );
  }

  renderParams(func) {
    const { parameterNames = [] } = func;

    return parameterNames
      .filter(Boolean)
      .map((param, i, arr) => {
        const elements = [
          span(
            {
              className: "param",
              key: param,
            },
            param
          ),
        ];
        // if this isn't the last param, add a comma
        if (i !== arr.length - 1) {
          elements.push(
            span(
              {
                className: "delimiter",
                key: i,
              },
              ", "
            )
          );
        }
        return elements;
      })
      .flat();
  }

  jumpToDefinitionButton(func) {
    const { location } = func;

    if (!location?.url || IGNORED_SOURCE_URLS.includes(location.url)) {
      return null;
    }

    const lastIndex = location.url.lastIndexOf("/");
    return button({
      className: "jump-definition",
      draggable: "false",
      title: `${location.url.slice(lastIndex + 1)}:${location.line}`,
    });
  }

  render() {
    const { func } = this.props;
    return span(
      {
        className: "function-signature",
      },
      this.renderFunctionName(func),
      span(
        {
          className: "paren",
        },
        "("
      ),
      this.renderParams(func),
      span(
        {
          className: "paren",
        },
        ")"
      ),
      this.jumpToDefinitionButton(func)
    );
  }
}

PreviewFunction.contextTypes = {
  l10n: PropTypes.object,
};
