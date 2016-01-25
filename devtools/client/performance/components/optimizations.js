/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const STRINGS_URI = "chrome://devtools/locale/jit-optimizations.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const { assert } = require("devtools/shared/DevToolsUtils");
const { DOM: dom, createClass, createFactory, PropTypes } = require("devtools/client/shared/vendor/react");
const Tree = createFactory(require("../../shared/components/tree"));
const OptimizationsItem = createFactory(require("./optimizations-item"));
const FrameView = createFactory(require("../../shared/components/frame"));

const onClickTooltipString = frame =>
  L10N.getFormatStr("viewsourceindebugger",`${frame.source}:${frame.line}:${frame.column}`);
const JIT_TITLE = L10N.getStr("jit.title");
// If TREE_ROW_HEIGHT changes, be sure to change `var(--jit-tree-row-height)`
// in `devtools/client/themes/jit-optimizations.css`
const TREE_ROW_HEIGHT = 14;

const Optimizations = module.exports = createClass({
  displayName: "Optimizations",

  propTypes: {
    onViewSourceInDebugger: PropTypes.func.isRequired,
    frameData: PropTypes.object.isRequired,
    optimizationSites: PropTypes.array.isRequired,
  },

  getInitialState() {
    return {
      expanded: new Set()
    };
  },

  getDefaultProps() {
    return {};
  },

  render() {
    let header = this._createHeader(this.props);
    let tree = this._createTree(this.props);

    return dom.div({}, header, tree);
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
    let { frameData, onViewSourceInDebugger, optimizationSites: sites } = this.props;

    let getSite = id => sites.find(site => site.id === id);
    let getIonTypeForObserved = type =>
      getSite(type.id).data.types.find(iontype => (iontype.typeset || []).indexOf(type) !== -1);
    let isSite = site => getSite(site.id) === site;
    let isAttempts = attempts => getSite(attempts.id).data.attempts === attempts;
    let isAttempt = attempt => getSite(attempt.id).data.attempts.indexOf(attempt) !== -1;
    let isTypes = types => getSite(types.id).data.types === types;
    let isType = type => getSite(type.id).data.types.indexOf(type) !== -1;
    let isObservedType = type => getIonTypeForObserved(type);

    let getRowType = node => {
      return isSite(node) ? "site" :
             isAttempts(node) ? "attempts" :
             isTypes(node) ? "types" :
             isAttempt(node) ? "attempt" :
             isType(node) ? "type":
             isObservedType(node) ? "observedtype": null;
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
    };

    return Tree({
      autoExpandDepth: 0,
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
        } else {
          return [];
        }
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
  }
});
