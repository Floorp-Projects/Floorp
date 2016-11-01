/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/locale/jit-optimizations.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const { assert } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const Tree = createFactory(require("../../shared/components/tree"));
const OptimizationsItem = createFactory(require("./jit-optimizations-item"));
const FrameView = createFactory(require("../../shared/components/frame"));
const JIT_TITLE = L10N.getStr("jit.title");
// If TREE_ROW_HEIGHT changes, be sure to change `var(--jit-tree-row-height)`
// in `devtools/client/themes/jit-optimizations.css`
const TREE_ROW_HEIGHT = 14;

/* eslint-disable no-unused-vars */
/**
 * TODO - Re-enable this eslint rule. The JIT tool is a work in progress, and isn't fully
 *        integrated as of yet, and this may represent intended functionality.
 */
const onClickTooltipString = frame =>
  L10N.getFormatStr("viewsourceindebugger",
                    `${frame.source}:${frame.line}:${frame.column}`);
/* eslint-enable no-unused-vars */

const optimizationAttemptModel = {
  id: PropTypes.number.isRequired,
  strategy: PropTypes.string.isRequired,
  outcome: PropTypes.string.isRequired,
};

const optimizationObservedTypeModel = {
  keyedBy: PropTypes.string.isRequired,
  name: PropTypes.string,
  location: PropTypes.string,
  line: PropTypes.string,
};

const optimizationIonTypeModel = {
  id: PropTypes.number.isRequired,
  typeset: PropTypes.arrayOf(optimizationObservedTypeModel),
  site: PropTypes.number.isRequired,
  mirType: PropTypes.number.isRequired,
};

const optimizationSiteModel = {
  id: PropTypes.number.isRequired,
  propertyName: PropTypes.string,
  line: PropTypes.number.isRequired,
  column: PropTypes.number.isRequired,
  data: PropTypes.shape({
    attempts: PropTypes.arrayOf(optimizationAttemptModel).isRequired,
    types: PropTypes.arrayOf(optimizationIonTypeModel).isRequired,
  }).isRequired,
};

const JITOptimizations = createClass({
  displayName: "JITOptimizations",

  propTypes: {
    onViewSourceInDebugger: PropTypes.func.isRequired,
    frameData: PropTypes.object.isRequired,
    optimizationSites: PropTypes.arrayOf(optimizationSiteModel).isRequired,
    autoExpandDepth: PropTypes.number,
  },

  getDefaultProps() {
    return {
      autoExpandDepth: 0
    };
  },

  getInitialState() {
    return {
      expanded: new Set()
    };
  },

  /**
   * Frame data generated from `frameNode.getInfo()`, or an empty
   * object, as well as a handler for clicking on the frame component.
   *
   * @param {?Object} .frameData
   * @param {Function} .onViewSourceInDebugger
   * @return {ReactElement}
   */
  _createHeader: function ({ frameData, onViewSourceInDebugger }) {
    let { isMetaCategory, url, line } = frameData;
    let name = isMetaCategory ? frameData.categoryData.label :
               frameData.functionName || "";

    // Simulate `SavedFrame`s interface
    let frame = { source: url, line: +line, functionDisplayName: name };

    // Neither Meta Category nodes, or the lack of a selected frame node,
    // renders out a frame source, like "file.js:123"; so just use
    // an empty span.
    let frameComponent;
    if (isMetaCategory || !name) {
      frameComponent = dom.span();
    } else {
      frameComponent = FrameView({
        frame,
        onClick: () => onViewSourceInDebugger(frame),
      });
    }

    return dom.div({ className: "optimization-header" },
      dom.span({ className: "header-title" }, JIT_TITLE),
      dom.span({ className: "header-function-name" }, name),
      frameComponent
    );
  },

  _createTree(props) {
    let {
      autoExpandDepth,
      frameData,
      onViewSourceInDebugger,
      optimizationSites: sites
    } = this.props;

    let getSite = id => sites.find(site => site.id === id);
    let getIonTypeForObserved = type => {
      return getSite(type.id).data.types
        .find(iontype => (iontype.typeset || [])
        .indexOf(type) !== -1);
    };
    let isSite = site => getSite(site.id) === site;
    let isAttempts = attempts => getSite(attempts.id).data.attempts === attempts;
    let isAttempt = attempt => getSite(attempt.id).data.attempts.indexOf(attempt) !== -1;
    let isTypes = types => getSite(types.id).data.types === types;
    let isType = type => getSite(type.id).data.types.indexOf(type) !== -1;
    let isObservedType = type => getIonTypeForObserved(type);

    let getRowType = node => {
      if (isSite(node)) {
        return "site";
      }
      if (isAttempts(node)) {
        return "attempts";
      }
      if (isTypes(node)) {
        return "types";
      }
      if (isAttempt(node)) {
        return "attempt";
      }
      if (isType(node)) {
        return "type";
      }
      if (isObservedType(node)) {
        return "observedtype";
      }
      return null;
    };

    // Creates a unique key for each node in the
    // optimizations data
    let getKey = node => {
      let site = getSite(node.id);
      if (isSite(node)) {
        return node.id;
      } else if (isAttempts(node)) {
        return `${node.id}-A`;
      } else if (isTypes(node)) {
        return `${node.id}-T`;
      } else if (isType(node)) {
        return `${node.id}-T-${site.data.types.indexOf(node)}`;
      } else if (isAttempt(node)) {
        return `${node.id}-A-${site.data.attempts.indexOf(node)}`;
      } else if (isObservedType(node)) {
        let iontype = getIonTypeForObserved(node);
        return `${getKey(iontype)}-O-${iontype.typeset.indexOf(node)}`;
      }
      return "";
    };

    return Tree({
      autoExpandDepth,
      getParent: node => {
        let site = getSite(node.id);
        let parent;
        if (isAttempts(node) || isTypes(node)) {
          parent = site;
        } else if (isType(node)) {
          parent = site.data.types;
        } else if (isAttempt(node)) {
          parent = site.data.attempts;
        } else if (isObservedType(node)) {
          parent = getIonTypeForObserved(node);
        }
        assert(parent, "Could not find a parent for optimization data node");

        return parent;
      },
      getChildren: node => {
        if (isSite(node)) {
          return [node.data.types, node.data.attempts];
        } else if (isAttempts(node) || isTypes(node)) {
          return node;
        } else if (isType(node)) {
          return node.typeset || [];
        }
        return [];
      },
      isExpanded: node => this.state.expanded.has(node),
      onExpand: node => this.setState(state => {
        let expanded = new Set(state.expanded);
        expanded.add(node);
        return { expanded };
      }),
      onCollapse: node => this.setState(state => {
        let expanded = new Set(state.expanded);
        expanded.delete(node);
        return { expanded };
      }),
      onFocus: function () {},
      getKey,
      getRoots: () => sites || [],
      itemHeight: TREE_ROW_HEIGHT,
      renderItem: (item, depth, focused, arrow, expanded) =>
        new OptimizationsItem({
          onViewSourceInDebugger,
          item,
          depth,
          focused,
          arrow,
          expanded,
          type: getRowType(item),
          frameData,
        }),
    });
  },

  render() {
    let header = this._createHeader(this.props);
    let tree = this._createTree(this.props);

    return dom.div({}, header, tree);
  }
});

module.exports = JITOptimizations;
