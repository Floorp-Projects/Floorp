/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gMIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

const gExternalProtocolService = Cc[
  "@mozilla.org/uriloader/external-protocol-service;1"
].getService(Ci.nsIExternalProtocolService);

const gHandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);

// This seems odd, but for test purposes, this just has to be a file that we know exists,
// and by using this file, we don't have to worry about different platforms.
let exeFile = Services.dirsvc.get("XREExeF", Ci.nsIFile);

add_task(async function test_valid_handlers() {
  await setupPolicyEngineWithJson({
    policies: {
      Handlers: {
        mimeTypes: {
          "application/marimba": {
            action: "useHelperApp",
            ask: true,
            handlers: [
              {
                name: "Launch",
                path: exeFile.path,
              },
            ],
          },
        },
        schemes: {
          fake_scheme: {
            action: "useHelperApp",
            ask: false,
            handlers: [
              {
                name: "Name",
                uriTemplate: "https://www.example.org/?%s",
              },
            ],
          },
        },
        extensions: {
          txt: {
            action: "saveToDisk",
            ask: false,
          },
        },
      },
    },
  });

  let handlerInfo = gMIMEService.getFromTypeAndExtension(
    "application/marimba",
    ""
  );
  is(handlerInfo.preferredAction, handlerInfo.useHelperApp);
  is(handlerInfo.alwaysAskBeforeHandling, true);
  is(handlerInfo.preferredApplicationHandler.name, "Launch");
  is(handlerInfo.preferredApplicationHandler.executable.path, exeFile.path);

  handlerInfo.preferredApplicationHandler = null;
  gHandlerService.store(handlerInfo);

  handlerInfo = handlerInfo = gMIMEService.getFromTypeAndExtension(
    "application/marimba",
    ""
  );
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);

  handlerInfo = gExternalProtocolService.getProtocolHandlerInfo("fake_scheme");
  is(handlerInfo.preferredAction, handlerInfo.useHelperApp);
  is(handlerInfo.alwaysAskBeforeHandling, false);
  is(handlerInfo.preferredApplicationHandler.name, "Name");
  is(
    handlerInfo.preferredApplicationHandler.uriTemplate,
    "https://www.example.org/?%s"
  );

  handlerInfo.preferredApplicationHandler = null;
  gHandlerService.store(handlerInfo);

  handlerInfo = gExternalProtocolService.getProtocolHandlerInfo("fake_scheme");
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);

  handlerInfo = gMIMEService.getFromTypeAndExtension("", "txt");
  is(handlerInfo.preferredAction, handlerInfo.saveToDisk);
  is(handlerInfo.alwaysAskBeforeHandling, false);

  handlerInfo.preferredApplicationHandler = null;
  gHandlerService.store(handlerInfo);
  handlerInfo = gMIMEService.getFromTypeAndExtension("", "txt");
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);
});

add_task(async function test_no_handler() {
  await setupPolicyEngineWithJson({
    policies: {
      Handlers: {
        schemes: {
          no_handler: {
            action: "useHelperApp",
          },
        },
      },
    },
  });

  let handlerInfo =
    gExternalProtocolService.getProtocolHandlerInfo("no_handler");
  is(handlerInfo.preferredAction, handlerInfo.alwaysAsk);
  is(handlerInfo.alwaysAskBeforeHandling, true);
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);
});

add_task(async function test_bad_web_handler1() {
  await setupPolicyEngineWithJson({
    policies: {
      Handlers: {
        schemes: {
          bas_web_handler1: {
            action: "useHelperApp",
            handlers: [
              {
                name: "Name",
                uriTemplate: "http://www.example.org/?%s",
              },
            ],
          },
        },
      },
    },
  });

  let handlerInfo =
    gExternalProtocolService.getProtocolHandlerInfo("bad_web_handler1");
  is(handlerInfo.preferredAction, handlerInfo.alwaysAsk);
  is(handlerInfo.alwaysAskBeforeHandling, true);
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);
});

add_task(async function test_bad_web_handler2() {
  await setupPolicyEngineWithJson({
    policies: {
      Handlers: {
        schemes: {
          bas_web_handler1: {
            action: "useHelperApp",
            handlers: [
              {
                name: "Name",
                uriTemplate: "http://www.example.org/",
              },
            ],
          },
        },
      },
    },
  });

  let handlerInfo =
    gExternalProtocolService.getProtocolHandlerInfo("bad_web_handler1");
  is(handlerInfo.preferredAction, handlerInfo.alwaysAsk);
  is(handlerInfo.alwaysAskBeforeHandling, true);
  is(handlerInfo.preferredApplicationHandler, null);

  gHandlerService.remove(handlerInfo);
});
