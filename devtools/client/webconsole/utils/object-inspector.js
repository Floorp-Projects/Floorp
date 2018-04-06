/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const ObjectClient = require("devtools/shared/client/object-client");

const reps = require("devtools/client/shared/components/reps/reps");
const { REPS, MODE } = reps;
const ObjectInspector = createFactory(reps.ObjectInspector);
const { Grip } = REPS;

/**
 * Create and return an ObjectInspector for the given grip.
 *
 * @param {Object} grip
 *        The object grip to create an ObjectInspector for.
 * @param {Object} serviceContainer
 *        Object containing various utility functions
 * @param {Object} override
 *        Object containing props that should override the default props passed to
 *        ObjectInspector.
 * @returns {ObjectInspector}
 *        An ObjectInspector for the given grip.
 */
function getObjectInspector(grip, serviceContainer, override) {
  let onDOMNodeMouseOver;
  let onDOMNodeMouseOut;
  let onInspectIconClick;
  if (serviceContainer) {
    onDOMNodeMouseOver = serviceContainer.highlightDomElement
      ? (object) => serviceContainer.highlightDomElement(object)
      : null;
    onDOMNodeMouseOut = serviceContainer.unHighlightDomElement;
    onInspectIconClick = serviceContainer.openNodeInInspector
      ? (object, e) => {
        // Stop the event propagation so we don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        serviceContainer.openNodeInInspector(object);
      }
      : null;
  }

  const objectInspectorProps = {
    autoExpandDepth: 0,
    mode: MODE.LONG,
    // TODO: we disable focus since it's not currently working well in ObjectInspector.
    // Let's remove the property below when problem are fixed in OI.
    disabledFocus: true,
    roots: [{
      path: (grip && grip.actor) || JSON.stringify(grip),
      contents: {
        value: grip
      }
    }],
    createObjectClient: object =>
      new ObjectClient(serviceContainer.hudProxy.client, object),
    releaseActor: actor => {
      if (!actor || !serviceContainer.hudProxy.releaseActor) {
        return;
      }
      serviceContainer.hudProxy.releaseActor(actor);
    },
    onViewSourceInDebugger: serviceContainer.onViewSourceInDebugger,
    openLink: serviceContainer.openLink,
  };

  if (!(typeof grip === "string" || (grip && grip.type === "longString"))) {
    Object.assign(objectInspectorProps, {
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      onInspectIconClick,
      defaultRep: Grip,
    });
  }

  return ObjectInspector({...objectInspectorProps, ...override});
}

exports.getObjectInspector = getObjectInspector;
