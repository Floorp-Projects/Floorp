/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createElement,
} = require("resource://devtools/client/shared/vendor/react.js");

loader.lazyGetter(this, "REPS", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .REPS;
});
loader.lazyGetter(this, "MODE", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .MODE;
});
loader.lazyGetter(this, "ObjectInspector", function () {
  const reps = require("resource://devtools/client/shared/components/reps/index.js");
  return createFactory(reps.objectInspector.ObjectInspector);
});

loader.lazyRequireGetter(
  this,
  "SmartTrace",
  "resource://devtools/client/shared/components/SmartTrace.js"
);

loader.lazyRequireGetter(
  this,
  "LongStringFront",
  "resource://devtools/client/fronts/string.js",
  true
);

loader.lazyRequireGetter(
  this,
  "ObjectFront",
  "resource://devtools/client/fronts/object.js",
  true
);

/**
 * Create and return an ObjectInspector for the given front.
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
function getObjectInspector(
  frontOrPrimitiveGrip,
  serviceContainer,
  override = {}
) {
  let onDOMNodeMouseOver;
  let onDOMNodeMouseOut;
  let onInspectIconClick;

  if (serviceContainer) {
    onDOMNodeMouseOver = serviceContainer.highlightDomElement
      ? object => serviceContainer.highlightDomElement(object)
      : null;
    onDOMNodeMouseOut = serviceContainer.unHighlightDomElement
      ? object => serviceContainer.unHighlightDomElement(object)
      : null;
    onInspectIconClick = serviceContainer.openNodeInInspector
      ? (object, e) => {
          // Stop the event propagation so we don't trigger ObjectInspector expand/collapse.
          e.stopPropagation();
          serviceContainer.openNodeInInspector(object);
        }
      : null;
  }

  const roots = createRoots(frontOrPrimitiveGrip, override.pathPrefix);

  const objectInspectorProps = {
    autoExpandDepth: 0,
    mode: MODE.LONG,
    standalone: true,
    roots,
    onViewSourceInDebugger: serviceContainer.onViewSourceInDebugger,
    recordTelemetryEvent: serviceContainer.recordTelemetryEvent,
    openLink: serviceContainer.openLink,
    sourceMapURLService: serviceContainer.sourceMapURLService,
    customFormat: override.customFormat !== false,
    setExpanded: override.setExpanded,
    initiallyExpanded: override.initiallyExpanded,
    queueActorsForCleanup: override.queueActorsForCleanup,
    cachedNodes: override.cachedNodes,
    urlCropLimit: 120,
    renderStacktrace: stacktrace => {
      const attrs = {
        key: "stacktrace",
        stacktrace,
        onViewSourceInDebugger: serviceContainer
          ? serviceContainer.onViewSourceInDebugger ||
            serviceContainer.onViewSource
          : null,
        onViewSource: serviceContainer.onViewSource,
        onReady: override.maybeScrollToBottom,
        sourceMapURLService: serviceContainer
          ? serviceContainer.sourceMapURLService
          : null,
      };

      if (serviceContainer?.preventStacktraceInitialRenderDelay) {
        attrs.initialRenderDelay = 0;
      }
      return createElement(SmartTrace, attrs);
    },
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick,
    defaultRep: REPS.Grip,
    createElement: serviceContainer?.createElement,
    mayUseCustomFormatter: true,
    ...override,
  };

  if (override.autoFocusRoot) {
    Object.assign(objectInspectorProps, {
      focusedItem: objectInspectorProps.roots[0],
    });
  }

  return ObjectInspector(objectInspectorProps);
}

function createRoots(frontOrPrimitiveGrip, pathPrefix = "") {
  const isFront =
    frontOrPrimitiveGrip instanceof ObjectFront ||
    frontOrPrimitiveGrip instanceof LongStringFront;
  const grip = isFront ? frontOrPrimitiveGrip.getGrip() : frontOrPrimitiveGrip;

  return [
    {
      path: `${pathPrefix}${
        frontOrPrimitiveGrip
          ? frontOrPrimitiveGrip.actorID || frontOrPrimitiveGrip.actor
          : null
      }`,
      contents: { value: grip, front: isFront ? frontOrPrimitiveGrip : null },
    },
  ];
}

module.exports = {
  getObjectInspector,
};
