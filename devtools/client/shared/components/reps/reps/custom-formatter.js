/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  // Dependencies
  const {
    Component,
    createElement,
    createFactory,
  } = require("devtools/client/shared/vendor/react");
  const {
    cleanupStyle,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const flags = require("resource://devtools/shared/flags.js");

  const ALLOWED_TAGS = new Set([
    "span",
    "div",
    "ol",
    "ul",
    "li",
    "table",
    "tr",
    "td",
  ]);

  class CustomFormatter extends Component {
    static get propTypes() {
      return {
        autoExpandDepth: PropTypes.number,
        client: PropTypes.object,
        createElement: PropTypes.func,
        frame: PropTypes.object,
        front: PropTypes.object,
        object: PropTypes.object.isRequired,
      };
    }

    constructor(props) {
      super(props);
      this.state = { open: false };
      this.toggleBody = this.toggleBody.bind(this);
    }

    componentDidThrow(e) {
      console.error("Error in CustomFormatter", e);
      this.setState(state => ({ ...state, hasError: true }));
    }

    async toggleBody(evt) {
      evt.stopPropagation();

      const open = !this.state.open;
      if (open && !this.state.bodyJsonMl) {
        let front = this.props.front;
        if (!front && this.props.client?.createObjectFront) {
          if (flags.testing && !this.props.frame) {
            throw new Error("props.frame is mandatory");
          }
          front = this.props.client.createObjectFront(
            this.props.object,
            this.props.frame
          );
        }
        if (!front) {
          return;
        }

        const response = await front.customFormatterBody();

        const bodyJsonMl = renderJsonMl(response.customFormatterBody, {
          ...this.props,
          autoExpandDepth: this.props.autoExpandDepth
            ? this.props.autoExpandDepth - 1
            : 0,
          object: null,
        });

        this.setState(state => ({
          ...state,
          bodyJsonMl,
          open,
        }));
      } else {
        this.setState(state => ({
          ...state,
          bodyJsonMl: null,
          open,
        }));
      }
    }

    render() {
      if (this.state && this.state.hasError) {
        return createElement(
          "span",
          {
            className: "objectBox objectBox-failure",
            title:
              "This object could not be rendered, " +
              "please file a bug on bugzilla.mozilla.org",
          },
          "Invalid custom formatter object"
        );
      }

      const headerJsonMl = renderJsonMl(this.props.object.header, {
        ...this.props,
        open: this.state?.open,
      });

      return createElement(
        "span",
        {
          className: "objectBox-jsonml-wrapper",
          "data-expandable": this.props.object.hasBody,
          "aria-expanded": this.state.open,
          onClick: this.props.object.hasBody ? this.toggleBody : null,
        },
        headerJsonMl,
        this.state.bodyJsonMl
          ? createElement(
              "div",
              { className: "objectBox-jsonml-body-wrapper" },
              this.state.bodyJsonMl
            )
          : null
      );
    }
  }

  function renderJsonMl(jsonMl, props, index = 0) {
    // The second item of the array can either be an object holding the attributes
    // for the element or the first child element. Therefore, all array items after the
    // first one are fetched together and split afterwards if needed.
    let [tagName, ...attributesAndChildren] = jsonMl ?? [];

    if (!ALLOWED_TAGS.has(tagName)) {
      tagName = "div";
    }

    const attributes = attributesAndChildren[0];
    const hasAttributes =
      Object(attributes) === attributes && !Array.isArray(attributes);
    const style =
      hasAttributes && attributes?.style && props.createElement
        ? cleanupStyle(attributes.style, props.createElement)
        : null;
    const children = attributesAndChildren;
    if (hasAttributes) {
      children.shift();
    }

    const childElements = [];

    if (props.object?.hasBody) {
      childElements.push(
        createElement("button", {
          "aria-expanded": props.open,
          className: `collapse-button jsonml-header-collapse-button${
            props.open ? " expanded" : ""
          }`,
        })
      );
    }

    if (Array.isArray(children)) {
      children.forEach((child, childIndex) => {
        let childElement;
        // If the child is an array, it should be a JsonML item, so use this function to
        // render them.
        if (Array.isArray(child)) {
          childElement = renderJsonMl(
            child,
            { ...props, object: null },
            childIndex
          );
        } else if (typeof child === "object" && child !== null) {
          // If we don't have an array, this means that we're probably dealing with
          // a front or a grip. If the object has a `getGrip` function, call it to get the
          // actual grip.
          const gripOrPrimitive =
            (child.typeName == "obj" || child.typeName == "string") &&
            typeof child?.getGrip == "function"
              ? child.getGrip()
              : child;

          // If the grip represents an object that was custom formatted, we should render
          // it using this component.
          if (supportsObject(gripOrPrimitive)) {
            childElement = createElement(CustomFormatter, {
              ...props,
              object: gripOrPrimitive,
              front: child && !!child.typeName ? child : null,
            });
          } else {
            // Here we have a non custom-formatted grip, so we let the ObjectInspector
            // handles it.
            const {
              objectInspector,
              MODE,
            } = require("devtools/client/shared/components/reps/index");
            childElement = createElement(objectInspector.ObjectInspector, {
              ...props,
              mode: props.mode == MODE.LONG ? MODE.SHORT : MODE.TINY,
              roots: [
                {
                  path: `${
                    gripOrPrimitive?.actorID ?? gripOrPrimitive?.actor ?? null
                  }`,
                  contents: {
                    value: gripOrPrimitive,
                    front: child && !!child.typeName ? child : null,
                  },
                },
              ],
            });
          }
        } else {
          // Here we have a primitive. We don't want to use Rep to render them as reps come
          // with their own styling which might clash with the style defined in the JsonMl.
          childElement = child;
        }
        childElements.push(childElement);
      });
    } else {
      childElements.push(children);
    }

    return createElement(
      tagName,
      {
        className: `objectBox objectBox-jsonml`,
        key: `jsonml-${tagName}-${index}`,
        style,
      },
      childElements
    );
  }

  function supportsObject(grip) {
    return grip?.useCustomFormatter === true && Array.isArray(grip?.header);
  }

  // Exports from this module
  module.exports = {
    rep: createFactory(CustomFormatter),
    supportsObject,
  };
});
