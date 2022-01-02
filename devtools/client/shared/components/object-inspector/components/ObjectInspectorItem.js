/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Services = require("Services");
const isMacOS = Services.appinfo.OS === "Darwin";

const {
  MODE,
} = require("devtools/client/shared/components/reps/reps/constants");

const Utils = require("devtools/client/shared/components/object-inspector/utils/index");

const {
  getValue,
  nodeHasAccessors,
  nodeHasProperties,
  nodeIsBlock,
  nodeIsDefaultProperties,
  nodeIsFunction,
  nodeIsGetter,
  nodeIsMapEntry,
  nodeIsMissingArguments,
  nodeIsOptimizedOut,
  nodeIsPrimitive,
  nodeIsPrototype,
  nodeIsSetter,
  nodeIsUninitializedBinding,
  nodeIsUnmappedBinding,
  nodeIsUnscopedBinding,
  nodeIsWindow,
  nodeIsLongString,
  nodeHasFullText,
  nodeHasGetter,
  getNonPrototypeParentGripValue,
} = Utils.node;

class ObjectInspectorItem extends Component {
  static get defaultProps() {
    return {
      onContextMenu: () => {},
      renderItemActions: () => null,
    };
  }

  // eslint-disable-next-line complexity
  getLabelAndValue() {
    const { item, depth, expanded, mode } = this.props;

    const label = item.name;
    const isPrimitive = nodeIsPrimitive(item);

    if (nodeIsOptimizedOut(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(optimized away)"),
      };
    }

    if (nodeIsUninitializedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(uninitialized)"),
      };
    }

    if (nodeIsUnmappedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unmapped)"),
      };
    }

    if (nodeIsUnscopedBinding(item)) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unscoped)"),
      };
    }

    const itemValue = getValue(item);
    const unavailable =
      isPrimitive &&
      itemValue &&
      itemValue.hasOwnProperty &&
      itemValue.hasOwnProperty("unavailable");

    if (nodeIsMissingArguments(item) || unavailable) {
      return {
        label,
        value: dom.span({ className: "unavailable" }, "(unavailable)"),
      };
    }

    if (
      nodeIsFunction(item) &&
      !nodeIsGetter(item) &&
      !nodeIsSetter(item) &&
      (mode === MODE.TINY || !mode)
    ) {
      return {
        label: Utils.renderRep(item, {
          ...this.props,
          functionName: label,
        }),
      };
    }

    if (
      nodeHasProperties(item) ||
      nodeHasAccessors(item) ||
      nodeIsMapEntry(item) ||
      nodeIsLongString(item) ||
      isPrimitive
    ) {
      const repProps = { ...this.props };
      if (depth > 0) {
        repProps.mode = mode === MODE.LONG ? MODE.SHORT : MODE.TINY;
      }
     

      if (nodeIsLongString(item)) {
        repProps.member = {
          open: nodeHasFullText(item) && expanded,
        };
      }

      if (nodeHasGetter(item)) {
        const receiverGrip = getNonPrototypeParentGripValue(item);
        if (receiverGrip) {
          Object.assign(repProps, {
            onInvokeGetterButtonClick: () =>
              this.props.invokeGetter(item, receiverGrip.actor),
          });
        }
      }

      return {
        label,
        value: Utils.renderRep(item, repProps),
      };
    }

    return {
      label,
    };
  }

  getTreeItemProps() {
    const {
      item,
      depth,
      focused,
      expanded,
      onCmdCtrlClick,
      onDoubleClick,
      dimTopLevelWindow,
      onContextMenu,
    } = this.props;

    const classNames = ["node", "object-node"];
    if (focused) {
      classNames.push("focused");
    }

    if (nodeIsBlock(item)) {
      classNames.push("block");
    }

    if (
      !expanded &&
      (nodeIsDefaultProperties(item) ||
        nodeIsPrototype(item) ||
        nodeIsGetter(item) ||
        nodeIsSetter(item) ||
        (dimTopLevelWindow === true && nodeIsWindow(item) && depth === 0))
    ) {
      classNames.push("lessen");
    }

    const parentElementProps = {
      className: classNames.join(" "),
      onClick: e => {
        if (
          onCmdCtrlClick &&
          ((isMacOS && e.metaKey) || (!isMacOS && e.ctrlKey))
        ) {
          onCmdCtrlClick(item, {
            depth,
            event: e,
            focused,
            expanded,
          });
          e.stopPropagation();
          return;
        }

        // If this click happened because the user selected some text, bail out.
        // Note that if the user selected some text before and then clicks here,
        // the previously selected text will be first unselected, unless the
        // user clicked on the arrow itself. Indeed because the arrow is an
        // image, clicking on it does not remove any existing text selection.
        // So we need to also check if the arrow was clicked.
        if (
          e.target &&
          Utils.selection.documentHasSelection(e.target.ownerDocument) &&
          !(e.target.matches && e.target.matches(".arrow"))
        ) {
          e.stopPropagation();
        }
      },
      onContextMenu: e => onContextMenu(e, item),
    };

    if (onDoubleClick) {
      parentElementProps.onDoubleClick = e => {
        e.stopPropagation();
        onDoubleClick(item, {
          depth,
          focused,
          expanded,
        });
      };
    }

    return parentElementProps;
  }

  renderLabel(label) {
    if (label === null || typeof label === "undefined") {
      return null;
    }

    const { item, depth, focused, expanded, onLabelClick } = this.props;
    return dom.span(
      {
        className: "object-label",
        onClick: onLabelClick
          ? event => {
              event.stopPropagation();

              // If the user selected text, bail out.
              if (
                Utils.selection.documentHasSelection(event.target.ownerDocument)
              ) {
                return;
              }

              onLabelClick(item, {
                depth,
                focused,
                expanded,
                setExpanded: this.props.setExpanded,
              });
            }
          : undefined,
      },
      label
    );
  }

  render() {
    const { arrow, renderItemActions, item } = this.props;

    const { label, value } = this.getLabelAndValue();
    const labelElement = this.renderLabel(label);
    const delimiter =
      value && labelElement
        ? dom.span({ className: "object-delimiter" }, ": ")
        : null;

    return dom.div(
      this.getTreeItemProps(),
      arrow,
      labelElement,
      delimiter,
      value,
      renderItemActions(item)
    );
  }
}

module.exports = ObjectInspectorItem;
