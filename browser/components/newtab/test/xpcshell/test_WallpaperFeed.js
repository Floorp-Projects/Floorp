/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WallpaperFeed } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/WallpaperFeed.sys.mjs"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  Utils: "resource://services-settings/Utils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const PREF_WALLPAPERS_ENABLED =
  "browser.newtabpage.activity-stream.newtabWallpapers.enabled";

add_task(async function test_construction() {
  let feed = new WallpaperFeed();

  info("WallpaperFeed constructor should create initial values");

  Assert.ok(feed, "Could construct a WallpaperFeed");
  Assert.ok(feed.loaded === false, "WallpaperFeed is not loaded");
  Assert.ok(
    feed.wallpaperClient === null,
    "wallpaperClient is initialized as null"
  );
  Assert.ok(
    feed.baseAttachmentURL === "",
    "baseAttachmentURL is initialized as an empty string"
  );
});

add_task(async function test_onAction_INIT() {
  let sandbox = sinon.createSandbox();
  let feed = new WallpaperFeed();
  Services.prefs.setBoolPref(PREF_WALLPAPERS_ENABLED, true);
  const attachment = {
    attachment: {
      location: "attachment",
    },
  };
  sandbox.stub(feed, "RemoteSettings").returns({
    get: () => [attachment],
    on: () => {},
  });
  sandbox.stub(Utils, "SERVER_URL").returns("http://localhost:8888/v1");
  feed.store = {
    dispatch: sinon.spy(),
  };
  sandbox.stub(feed, "fetch").resolves({
    json: () => ({
      capabilities: {
        attachments: {
          base_url: "http://localhost:8888/base_url/",
        },
      },
    }),
  });

  info("WallpaperFeed.onAction INIT should initialize wallpapers");

  await feed.onAction({
    type: at.INIT,
  });

  Assert.ok(feed.store.dispatch.calledThrice);
  Assert.ok(
    feed.store.dispatch.firstCall.calledWith(
      ac.BroadcastToContent({
        type: at.WALLPAPERS_SET,
        data: [
          {
            ...attachment,
            wallpaperUrl: "http://localhost:8888/base_url/attachment",
            category: "other",
          },
        ],
        meta: {
          isStartup: true,
        },
      })
    )
  );
  Services.prefs.clearUserPref(PREF_WALLPAPERS_ENABLED);
  sandbox.restore();
});

add_task(async function test_onAction_PREF_CHANGED() {
  let sandbox = sinon.createSandbox();
  let feed = new WallpaperFeed();
  Services.prefs.setBoolPref(PREF_WALLPAPERS_ENABLED, true);
  sandbox.stub(feed, "wallpaperSetup").returns();

  info("WallpaperFeed.onAction PREF_CHANGED should call wallpaperSetup");

  feed.onAction({
    type: at.PREF_CHANGED,
    data: { name: "newtabWallpapers.enabled" },
  });

  Assert.ok(feed.wallpaperSetup.calledOnce);
  Assert.ok(feed.wallpaperSetup.calledWith(false));

  Services.prefs.clearUserPref(PREF_WALLPAPERS_ENABLED);
  sandbox.restore();
});
