/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/**
 * Responsible for fetching HTTP response content from the backend.
 *
 * @param {DevtoolsExtensionContext}
 *   A devtools extension context running in a child process.
 * @param {object} options
 */
class ChildNetworkResponseLoader {
  constructor(context, requestId) {
    this.context = context;
    this.requestId = requestId;
  }

  api() {
    const { context, requestId } = this;
    return {
      getContent(callback) {
        return context.childManager.callParentAsyncFunction(
          "devtools.network.Request.getContent",
          [requestId],
          callback
        );
      },
    };
  }
}

this.devtools_network = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {
        network: {
          onRequestFinished: new EventManager({
            context,
            name: "devtools.network.onRequestFinished",
            register: fire => {
              let onFinished = data => {
                const loader = new ChildNetworkResponseLoader(
                  context,
                  data.requestId
                );
                const harEntry = { ...data.harEntry, ...loader.api() };
                const result = Cu.cloneInto(harEntry, context.cloneScope, {
                  cloneFunctions: true,
                });
                fire.asyncWithoutClone(result);
              };

              let parent = context.childManager.getParentEvent(
                "devtools.network.onRequestFinished"
              );
              parent.addListener(onFinished);
              return () => {
                parent.removeListener(onFinished);
              };
            },
          }).api(),
        },
      },
    };
  }
};
