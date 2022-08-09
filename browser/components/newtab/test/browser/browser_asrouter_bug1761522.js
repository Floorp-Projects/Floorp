/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter, MessageLoaderUtils } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { RemoteL10n } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteL10n.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

// This pref is used to override the Remote Settings server URL in tests.
// See SERVER_URL in services/settings/Utils.jsm for more details.
const RS_SERVER_PREF = "services.settings.server";

const FLUENT_CONTENT = "asrouter-test-string = Test Test Test\n";

async function serveRemoteSettings() {
  const server = new HttpServer();
  server.start(-1);

  const baseURL = `http://localhost:${server.identity.primaryPort}/`;
  const attachmentUuid = crypto.randomUUID();
  const attachment = new TextEncoder().encode(FLUENT_CONTENT);

  // Serve an index so RS knows where to fetch images from.
  server.registerPathHandler("/v1/", (request, response) => {
    response.write(
      JSON.stringify({
        capabilities: {
          attachments: {
            base_url: `${baseURL}cdn`,
          },
        },
      })
    );
  });

  // Serve the ms-language-packs record for cfr-v1-ja-JP-mac, pointing to an attachment.
  server.registerPathHandler(
    "/v1/buckets/main/collections/ms-language-packs/records/cfr-v1-ja-JP-mac",
    (request, response) => {
      response.setStatusLine(null, 200, "OK");
      response.setHeader(
        "Content-type",
        "application/json; charset=utf-8",
        false
      );
      response.write(
        JSON.stringify({
          permissions: {},
          data: {
            attachment: {
              hash:
                "f9aead2693c4ff95c2764df72b43fdf5b3490ed06414588843848f991136040b",
              size: attachment.buffer.byteLength,
              filename: "asrouter.ftl",
              location: `main-workspace/ms-language-packs/${attachmentUuid}`,
            },
            id: "cfr-v1-ja-JP-mac",
            last_modified: Date.now(),
          },
        })
      );
    }
  );

  // Serve the attachment for ms-language-packs/cfr-va-ja-JP-mac.
  server.registerPathHandler(
    `/cdn/main-workspace/ms-language-packs/${attachmentUuid}`,
    (request, response) => {
      const stream = Cc[
        "@mozilla.org/io/arraybuffer-input-stream;1"
      ].createInstance(Ci.nsIArrayBufferInputStream);
      stream.setData(attachment.buffer, 0, attachment.buffer.byteLength);

      response.setStatusLine(null, 200, "OK");
      response.setHeader("Content-type", "application/octet-stream");
      response.bodyOutputStream.writeFrom(stream, attachment.buffer.byteLength);
    }
  );

  // Serve the list of changed collections. cfr must have changed, otherwise we
  // won't attempt to fetch the cfr records (and then won't fetch
  // ms-language-packs).
  server.registerPathHandler(
    "/v1/buckets/monitor/collections/changes/changeset",
    (request, response) => {
      const now = Date.now();
      response.setStatusLine(null, 200, "OK");
      response.setHeader(
        "Content-type",
        "application/json; charset=utf-8",
        false
      );
      response.write(
        JSON.stringify({
          timestamp: now,
          changes: [
            {
              host: `localhost:${server.identity.primaryPort}`,
              last_modified: now,
              bucket: "main",
              collection: "cfr",
            },
          ],
          metadata: {},
        })
      );
    }
  );

  const message = await PanelTestProvider.getMessages().then(msgs =>
    msgs.find(msg => msg.id === "PERSONALIZED_CFR_MESSAGE")
  );

  // Serve the "changed" cfr entries. If there are no changes, then ASRouter
  // won't re-fetch ms-language-packs.
  server.registerPathHandler(
    "/v1/buckets/main/collections/cfr/changeset",
    (request, response) => {
      const now = Date.now();
      response.setStatusLine(null, 200, "OK");
      response.setHeader(
        "Content-type",
        "application/json; charset=utf-8",
        false
      );
      response.write(
        JSON.stringify({
          timestamp: now,
          changes: [message],
          metadata: {},
        })
      );
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [[RS_SERVER_PREF, `${baseURL}v1`]],
  });

  return async () => {
    await new Promise(resolve => server.stop(() => resolve()));
    await SpecialPowers.popPrefEnv();
  };
}

add_task(async function test_asrouter() {
  const MS_LANGUAGE_PACKS_DIR = PathUtils.join(
    PathUtils.localProfileDir,
    "settings",
    "main",
    "ms-language-packs"
  );
  const sandbox = sinon.createSandbox();
  const stop = await serveRemoteSettings();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.cfr",
        JSON.stringify({
          id: "cfr",
          enabled: true,
          type: "remote-settings",
          bucket: "cfr",
          updateCyleInMs: 3600000,
        }),
      ],
    ],
  });
  const localeService = Services.locale;
  RemoteSettings("cfr").verifySignature = false;

  registerCleanupFunction(async () => {
    RemoteSettings("cfr").verifySignature = true;
    Services.locale = localeService;
    await SpecialPowers.popPrefEnv();
    await stop();
    sandbox.restore();
    await IOUtils.remove(MS_LANGUAGE_PACKS_DIR, { recursive: true });
    RemoteL10n.reloadL10n();
  });

  // We can't stub Services.locale.appLocaleAsBCP47 directly because its an
  // XPCOM_Native object.
  const fakeLocaleService = new Proxy(localeService, {
    get(obj, prop) {
      if (prop === "appLocaleAsBCP47") {
        return "ja-JP-macos";
      }
      return obj[prop];
    },
  });

  const localeSpy = sandbox.spy(MessageLoaderUtils, "locale", ["get"]);
  Services.locale = fakeLocaleService;

  const cfrProvider = ASRouter.state.providers.find(p => p.id === "cfr");
  await ASRouter.loadMessagesFromAllProviders([cfrProvider]);

  Assert.equal(
    Services.locale.appLocaleAsBCP47,
    "ja-JP-macos",
    "Locale service returns ja-JP-macos"
  );
  Assert.ok(localeSpy.get.called, "MessageLoaderUtils.locale getter called");
  Assert.ok(
    localeSpy.get.alwaysReturned("ja-JP-mac"),
    "MessageLoaderUtils.locale getter returned expected locale ja-JP-mac"
  );

  const path = PathUtils.join(
    MS_LANGUAGE_PACKS_DIR,
    "browser",
    "newtab",
    "asrouter.ftl"
  );
  Assert.ok(await IOUtils.exists(path), "asrouter.ftl was downloaded");
  Assert.equal(
    await IOUtils.readUTF8(path),
    FLUENT_CONTENT,
    "asrouter.ftl content matches expected"
  );
});
