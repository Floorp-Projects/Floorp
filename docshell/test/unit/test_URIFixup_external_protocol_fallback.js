/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test whether fallback mechanism is working if don't trust nsIExternalProtocolService.

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

add_task(async function setup() {
  info(
    "Prepare mock nsIExternalProtocolService whose externalProtocolHandlerExists returns always true"
  );
  const externalProtocolService = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  const mockId = MockRegistrar.register(
    "@mozilla.org/uriloader/external-protocol-service;1",
    {
      getProtocolHandlerInfo: scheme =>
        externalProtocolService.getProtocolHandlerInfo(scheme),
      externalProtocolHandlerExists: () => true,
      QueryInterface: ChromeUtils.generateQI(["nsIExternalProtocolService"]),
    }
  );
  const mockExternalProtocolService = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  Assert.ok(
    mockExternalProtocolService.externalProtocolHandlerExists("__invalid__"),
    "Mock service is working"
  );

  info("Register new dummy protocol");
  const dummyProtocolHandlerInfo = externalProtocolService.getProtocolHandlerInfo(
    "dummy"
  );
  const handlerService = Cc[
    "@mozilla.org/uriloader/handler-service;1"
  ].getService(Ci.nsIHandlerService);
  handlerService.store(dummyProtocolHandlerInfo);

  info("Prepare test search engine");
  await setupSearchService();
  await addTestEngines();
  await Services.search.setDefault(
    Services.search.getEngineByName(kSearchEngineID)
  );

  registerCleanupFunction(() => {
    handlerService.remove(dummyProtocolHandlerInfo);
    MockRegistrar.unregister(mockId);
  });
});

add_task(function basic() {
  const testData = [
    {
      input: "mailto:test@example.com",
      expected:
        isSupportedInHandlerService("mailto") ||
        // Thunderbird IS a mailto handler, it doesn't have handlers.
        AppConstants.MOZ_APP_NAME == "thunderbird"
          ? "mailto:test@example.com"
          : "http://mailto:test@example.com/",
    },
    {
      input: "keyword:search",
      expected: "https://www.example.org/?search=keyword%3Asearch",
    },
    {
      input: "dummy:protocol",
      expected: "dummy:protocol",
    },
  ];

  for (const { input, expected } of testData) {
    assertFixup(input, expected);
  }
});

function assertFixup(input, expected) {
  const { preferredURI } = Services.uriFixup.getFixupURIInfo(
    input,
    Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
  );
  Assert.equal(preferredURI.spec, expected);
}

function isSupportedInHandlerService(scheme) {
  const externalProtocolService = Cc[
    "@mozilla.org/uriloader/external-protocol-service;1"
  ].getService(Ci.nsIExternalProtocolService);
  const handlerService = Cc[
    "@mozilla.org/uriloader/handler-service;1"
  ].getService(Ci.nsIHandlerService);
  return handlerService.exists(
    externalProtocolService.getProtocolHandlerInfo(scheme)
  );
}
