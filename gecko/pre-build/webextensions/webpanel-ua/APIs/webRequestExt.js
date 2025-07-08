/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

const { WebRequest } = ChromeUtils.importESModule(
  "resource://gre/modules/WebRequest.sys.mjs"
);

this.webRequestExt = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    return {
      webRequestExt: {
        onBeforeRequest_webpanel_requestId: new EventManager({
          context,
          register: fire => {
            function listener(e) {
              if (e.bmsUseragent === true) {
                return fire.async(e.requestId);
              }
              return fire.async(null);
            }
            WebRequest.onBeforeRequest.addListener(listener, null, [
              "blocking",
            ]);
            return () => {
              WebRequest.onBeforeRequest.removeListener(listener, null, [
                "blocking",
              ]);
            };
          },
        }).api(),
      },
    };
  }
};
