/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/client/locales/jit-optimizations.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const { PluralForm } = require("devtools/shared/plural-form");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Frame = createFactory(require("devtools/client/shared/components/Frame"));
const PROPNAME_MAX_LENGTH = 4;
// If TREE_ROW_HEIGHT changes, be sure to change `var(--jit-tree-row-height)`
// in `devtools/client/themes/jit-optimizations.css`
const TREE_ROW_HEIGHT = 14;

const OPTIMIZATION_ITEM_TYPES = ["site", "attempts", "types", "attempt", "type",
                                 "observedtype"];

/* eslint-disable no-unused-vars */
/**
 * TODO - Re-enable this eslint rule. The JIT tool is a work in progress, and isn't fully
 *        integrated as of yet.
 */
const {
  JITOptimizations, hasSuccessfulOutcome, isSuccessfulOutcome,
} = require("devtools/client/performance/modules/logic/jit");
const OPTIMIZATION_FAILURE = L10N.getStr("jit.optimizationFailure");
const JIT_SAMPLES = L10N.getStr("jit.samples");
const JIT_TYPES = L10N.getStr("jit.types");
const JIT_ATTEMPTS = L10N.getStr("jit.attempts");

/* eslint-enable no-unused-vars */

class JITOptimizationsItem extends Component {
  static get propTypes() {
    return {
      onViewSourceInDebugger: PropTypes.func.isRequired,
      frameData: PropTypes.object.isRequired,
      type: PropTypes.oneOf(OPTIMIZATION_ITEM_TYPES).isRequired,
      depth: PropTypes.number.isRequired,
      arrow: PropTypes.element.isRequired,
      item: PropTypes.object,
      focused: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);
    this._renderSite = this._renderSite.bind(this);
    this._renderAttempts = this._renderAttempts.bind(this);
    this._renderTypes = this._renderTypes.bind(this);
    this._renderAttempt = this._renderAttempt.bind(this);
    this._renderType = this._renderType.bind(this);
    this._renderObservedType = this._renderObservedType.bind(this);
  }

  _renderSite({ item: site, onViewSourceInDebugger, frameData }) {
    const attempts = site.data.attempts;
    const lastStrategy = attempts[attempts.length - 1].strategy;
    let propString = "";
    const propertyName = site.data.propertyName;

    // Display property name if it exists
    if (propertyName) {
      if (propertyName.length > PROPNAME_MAX_LENGTH) {
        propString = ` (.${propertyName.substr(0, PROPNAME_MAX_LENGTH)}…)`;
      } else {
        propString = ` (.${propertyName})`;
      }
    }

    const sampleString = PluralForm.get(site.samples, JIT_SAMPLES)
      .replace("#1", site.samples);
    const text = dom.span(
      { className: "optimization-site-title" },
      `${lastStrategy}${propString} – (${sampleString})`
    );
    const frame = Frame({
      onClick: () => onViewSourceInDebugger(frameData.url,
        site.data.line, site.data.column),
      frame: {
        source: frameData.url,
        line: +site.data.line,
        column: site.data.column,
      },
    });
    const children = [text, frame];

    if (!hasSuccessfulOutcome(site)) {
      children.unshift(dom.span({ className: "opt-icon warning" }));
    }

    return dom.span({ className: "optimization-site" }, ...children);
  }

  _renderAttempts({ item: attempts }) {
    return dom.span({ className: "optimization-attempts" },
      `${JIT_ATTEMPTS} (${attempts.length})`
    );
  }

  _renderTypes({ item: types }) {
    return dom.span({ className: "optimization-types" },
      `${JIT_TYPES} (${types.length})`
    );
  }

  _renderAttempt({ item: attempt }) {
    const success = isSuccessfulOutcome(attempt.outcome);
    const { strategy, outcome } = attempt;
    return dom.span({ className: "optimization-attempt" },
      dom.span({ className: "optimization-strategy" }, strategy),
      " → ",
      dom.span({ className: `optimization-outcome ${success ? "success" : "failure"}` },
               outcome)
    );
  }

  _renderType({ item: type }) {
    return dom.span({ className: "optimization-ion-type" },
                    `${type.site}:${type.mirType}`);
  }

  _renderObservedType({ onViewSourceInDebugger, item: type }) {
    const children = [
      dom.span({ className: "optimization-observed-type-keyed" },
        `${type.keyedBy}${type.name ? ` → ${type.name}` : ""}`),
    ];

    // If we have a line and location, make a link to the debugger
    if (type.location && type.line) {
      children.push(
        Frame({
          onClick: () => onViewSourceInDebugger(type.location, type.line, type.column),
          frame: {
            source: type.location,
            line: type.line,
            column: type.column,
          },
        })
      );
    // Otherwise if we just have a location, it's probably just a memory location.
    } else if (type.location) {
      children.push(`@${type.location}`);
    }

    return dom.span({ className: "optimization-observed-type" }, ...children);
  }

  render() {
    /* eslint-disable no-unused-vars */
    /**
     * TODO - Re-enable this eslint rule. The JIT tool is a work in progress, and these
     *        undefined variables may represent intended functionality.
     */
    const {
      depth,
      arrow,
      type,
      // TODO - The following are currently unused.
      item,
      focused,
      frameData,
      onViewSourceInDebugger,
    } = this.props;
    /* eslint-enable no-unused-vars */

    let content;
    switch (type) {
      case "site": content = this._renderSite(this.props); break;
      case "attempts": content = this._renderAttempts(this.props); break;
      case "types": content = this._renderTypes(this.props); break;
      case "attempt": content = this._renderAttempt(this.props); break;
      case "type": content = this._renderType(this.props); break;
      case "observedtype": content = this._renderObservedType(this.props); break;
    }

    return dom.div(
      {
        className: `optimization-tree-item optimization-tree-item-${type}`,
        style: { marginInlineStart: depth * TREE_ROW_HEIGHT },
      },
      arrow,
      content
    );
  }
}

module.exports = JITOptimizationsItem;
