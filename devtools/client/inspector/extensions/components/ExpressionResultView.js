/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);

const Types = require("devtools/client/inspector/extensions/types");

const {
  REPS: { Grip },
  MODE,
  objectInspector: { ObjectInspector: ObjectInspectorClass },
} = require("devtools/client/shared/components/reps/index");

loader.lazyRequireGetter(
  this,
  "LongStringFront",
  "devtools/client/fronts/string",
  true
);

loader.lazyRequireGetter(
  this,
  "ObjectFront",
  "devtools/client/fronts/object",
  true
);

const ObjectInspector = createFactory(ObjectInspectorClass);

class ObjectValueGripView extends PureComponent {
  static get propTypes() {
    return {
      rootTitle: PropTypes.string,
      expressionResult: PropTypes.oneOfType([
        PropTypes.string,
        PropTypes.number,
        PropTypes.object,
      ]).isRequired,
      // Helpers injected as props by extension-sidebar.js.
      serviceContainer: PropTypes.shape(Types.serviceContainer).isRequired,
    };
  }

  render() {
    const { expressionResult, serviceContainer, rootTitle } = this.props;

    const isFront =
      expressionResult instanceof ObjectFront ||
      expressionResult instanceof LongStringFront;
    const grip = isFront ? expressionResult.getGrip() : expressionResult;

    const objectInspectorProps = {
      autoExpandDepth: 1,
      mode: MODE.SHORT,
      // TODO: we disable focus since it's not currently working well in ObjectInspector.
      // Let's remove the property below when problem are fixed in OI.
      disabledFocus: true,
      roots: [
        {
          path: expressionResult?.actorID || JSON.stringify(expressionResult),
          contents: { value: grip, front: isFront ? expressionResult : null },
        },
      ],
      // TODO: evaluate if there should also be a serviceContainer.openLink.
    };

    if (expressionResult?.actorID) {
      Object.assign(objectInspectorProps, {
        onDOMNodeMouseOver: serviceContainer.highlightDomElement,
        onDOMNodeMouseOut: serviceContainer.unHighlightDomElement,
        onInspectIconClick(object, e) {
          // Stop the event propagation so we don't trigger ObjectInspector
          // expand/collapse.
          e.stopPropagation();
          serviceContainer.openNodeInInspector(object);
        },
        defaultRep: Grip,
      });
    }

    if (rootTitle) {
      return Accordion({
        items: [
          {
            component: ObjectInspector,
            componentProps: objectInspectorProps,
            header: rootTitle,
            id: rootTitle.replace(/\s/g, "-"),
            opened: true,
          },
        ],
      });
    }

    return ObjectInspector(objectInspectorProps);
  }
}

module.exports = ObjectValueGripView;
