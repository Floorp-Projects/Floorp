// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  NativeAutofill,
  NativePlaces,
  NativeProvider,
  PlacesConnection,
  QueryContext,
  QueryResult,
  UrlbarModules,
} from "./WorkspaceBookmarkUrlbar.types.ts";
import type { WorkspaceBookmarksService } from "../WorkspaceBookmarks.sys.mts";

function modules(): UrlbarModules {
  const { PlacesUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: NativePlaces };
  const { WorkspaceBookmarks } = ChromeUtils.importESModule(
    "resource://noraneko/modules/WorkspaceBookmarks.sys.mjs",
  ) as { WorkspaceBookmarks: WorkspaceBookmarksService };
  const { UrlbarProviderPlaces } = ChromeUtils.importESModule(
    "moz-src:///browser/components/urlbar/UrlbarProviderPlaces.sys.mjs",
  ) as { UrlbarProviderPlaces: UrlbarModules["ProviderPlaces"] };
  const { UrlbarProviderAutofill } = ChromeUtils.importESModule(
    "moz-src:///browser/components/urlbar/UrlbarProviderAutofill.sys.mjs",
  ) as { UrlbarProviderAutofill: UrlbarModules["ProviderAutofill"] };
  const { UrlbarProviderBookmarkKeywords } = ChromeUtils.importESModule(
    "moz-src:///browser/components/urlbar/UrlbarProviderBookmarkKeywords.sys.mjs",
  ) as { UrlbarProviderBookmarkKeywords: UrlbarModules["ProviderKeywords"] };
  const { UrlbarQueryContext } = ChromeUtils.importESModule(
    "chrome://browser/content/urlbar/UrlbarQueryContext.mjs",
  ) as { UrlbarQueryContext: UrlbarModules["QueryContext"] };
  const { UrlbarTokenizer } = ChromeUtils.importESModule(
    "moz-src:///browser/components/urlbar/UrlbarTokenizer.sys.mjs",
  ) as { UrlbarTokenizer: Pick<UrlbarModules, "tokenize"> };
  const { UrlbarShared } = ChromeUtils.importESModule(
    "chrome://browser/content/urlbar/UrlbarShared.mjs",
  ) as { UrlbarShared: { RESULT_SOURCE: UrlbarModules["sources"] } };
  return {
    places: PlacesUtils,
    scopes: WorkspaceBookmarks,
    ProviderPlaces: UrlbarProviderPlaces,
    ProviderKeywords: UrlbarProviderBookmarkKeywords,
    ProviderAutofill: UrlbarProviderAutofill,
    QueryContext: UrlbarQueryContext,
    tokenize: (context) => UrlbarTokenizer.tokenize(context),
    sources: UrlbarShared.RESULT_SOURCE,
  };
}

function context(
  native: UrlbarModules,
  target: Window,
  searchString: string,
): QueryContext {
  const query = new native.QueryContext({
    searchString,
    allowAutofill: true,
    isPrivate: false,
    maxResults: 50,
    sapName: "urlbar",
    sources: [native.sources.HISTORY, native.sources.BOOKMARKS],
  });
  query.tokens = native.tokenize(query);
  query.floorpWorkspaceBookmarkContext = native.scopes.getContext(target);
  return query;
}

async function results(
  provider: NativeProvider,
  query: QueryContext,
): Promise<QueryResult[]> {
  provider.queryInstance = {};
  const output: QueryResult[] = [];
  try {
    await provider.startQuery(
      query,
      (_provider, result) => output.push(result),
    );
    return output;
  } finally {
    provider.cancelQuery?.();
  }
}

async function autofillRows(
  native: UrlbarModules,
  connection: PlacesConnection,
  target: Window,
  method: "_getOriginQuery" | "_getUrlQuery" | "_getAdaptiveHistoryQuery",
  searchString: string,
): Promise<Array<{ title: unknown; url: unknown }>> {
  const provider: NativeAutofill = new native.ProviderAutofill();
  provider._strippedPrefix = "";
  provider._searchString = searchString;
  const [sql, parameters] = provider[method](
    context(native, target, searchString),
  );
  assert(sql && parameters, `${method} produces a query for the fixture`);
  assert(
    "floorpVisibleGuids" in parameters,
    `${method} binds the visibility filter before SQL matching`,
  );
  const rows = await connection.execute(sql, parameters);
  return rows.map((row) => ({
    title: row.getResultByName("title"),
    url: row.getResultByName("url"),
  }));
}

async function testNativeProvidersRespectWorkspaceScope(): Promise<void> {
  const native = modules();
  await native.scopes.ready;
  const suffix = crypto.randomUUID().replaceAll("-", "");
  const secretTitle = `scope-secret-${suffix}`;
  const visibleTitle = `scope-visible-${suffix}`;
  const historyTitle = `scope-history-${suffix}`;
  const hiddenHost = `hidden-${suffix}.invalid`;
  const visibleHost = `visible-${suffix}.invalid`;
  const historyHost = `history-${suffix}.invalid`;
  const urls = [hiddenHost, visibleHost, historyHost].map(
    (host) => `https://${host}/path`,
  );
  const [hiddenUrl, visibleUrl, historyUrl] = urls;
  const hiddenKeyword = `hidden${suffix}`;
  const visibleKeyword = `visible${suffix}`;
  const workspace = crypto.randomUUID();
  const otherWorkspace = crypto.randomUUID();
  const browser = Services.wm.getMostRecentWindow("navigator:browser") as
    | Window
    | null;
  assert(browser, "a native browser supplies the fixture's privacy context");
  const target = {
    closed: false,
    isChromeWindow: true,
    docShell: browser.docShell,
  } as unknown as Window;
  const prefNames = [
    "floorp.workspaces.enabled",
    "floorp.workspaces.bookmarks.enabled",
  ];
  const oldPrefs = prefNames.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, true),
  }));
  const folders: string[] = [];
  let hiddenFolder: string | undefined;
  try {
    for (const name of prefNames) Services.prefs.setBoolPref(name, true);
    for (const title of ["Visible scope fixture", "Hidden scope fixture"]) {
      const folder = await native.places.bookmarks.insert({
        parentGuid: native.places.bookmarks.unfiledGuid,
        type: native.places.bookmarks.TYPE_FOLDER,
        title,
      });
      folders.push(folder.guid);
    }
    const [visibleFolder] = folders;
    hiddenFolder = folders[1];
    assert(visibleFolder && hiddenFolder, "both fixture folders exist");
    for (const url of [hiddenUrl, visibleUrl, historyUrl]) {
      await native.places.bookmarks.insert({
        parentGuid: hiddenFolder,
        title: secretTitle,
        url,
      });
    }
    await native.places.bookmarks.insert({
      parentGuid: visibleFolder,
      title: visibleTitle,
      url: visibleUrl,
    });
    await native.places.history.insert({
      url: historyUrl,
      title: historyTitle,
      visits: [{
        date: new Date(),
        transition: native.places.history.TRANSITIONS.TYPED,
      }],
    });
    await native.places.keywords.insert({
      keyword: hiddenKeyword,
      url: hiddenUrl,
    });
    await native.places.keywords.insert({
      keyword: visibleKeyword,
      url: visibleUrl,
    });
    const connection = await native.places.promiseLargeCacheDBConnection();
    await native.places.withConnectionWrapper(
      "Workspace bookmark URLbar fixtures",
      async (writable) => {
        for (const url of urls) {
          await writable.execute(
            `UPDATE moz_places SET frecency = 1000000000,
          alt_frecency = 1000000000 WHERE url_hash = hash(:url) AND url = :url`,
            { url },
          );
          await writable.execute(
            `UPDATE moz_origins SET frecency = 1000000000,
          alt_frecency = 1000000000 WHERE id = (
            SELECT origin_id FROM moz_places WHERE url = :url
          )`,
            { url },
          );
          await writable.execute(
            `INSERT OR REPLACE INTO moz_inputhistory(place_id, input, use_count)
          SELECT id, :input, 100 FROM moz_places WHERE url = :url`,
            { url, input: new URL(url).host },
          );
        }
      },
    );
    await native.scopes.refresh();
    await native.scopes.setFolderScope(hiddenFolder, [otherWorkspace]);
    native.scopes.setWindowContext(target, workspace, true);

    const secret = await results(
      new native.ProviderPlaces(),
      context(native, target, secretTitle),
    );
    assertEquals(
      secret.length,
      0,
      "hidden bookmark names cannot create matches",
    );
    const hidden = await results(
      new native.ProviderPlaces(),
      context(native, target, hiddenHost),
    );
    assertEquals(
      hidden.length,
      0,
      "hidden unvisited URLs cannot create matches",
    );
    const visible = await results(
      new native.ProviderPlaces(),
      context(native, target, visibleHost),
    );
    assert(
      visible.some((result) => result.payload.title === visibleTitle),
      "the visible GUID supplies the duplicate URL's bookmark name",
    );
    const history = await results(
      new native.ProviderPlaces(),
      context(native, target, historyHost),
    );
    assert(
      history.some((result) =>
        result.payload.title === historyTitle &&
        result.source === native.sources.HISTORY
      ),
      "independent visits keep their history title and history source",
    );

    for (
      const method of [
        "_getOriginQuery",
        "_getUrlQuery",
        "_getAdaptiveHistoryQuery",
      ] as const
    ) {
      const path = method === "_getOriginQuery" ? "" : "/pa";
      assertEquals(
        (await autofillRows(
          native,
          connection,
          target,
          method,
          hiddenHost + path,
        ))
          .length,
        0,
        `${method} rejects a hidden unvisited bookmark despite its frecency`,
      );
      for (const host of [visibleHost, historyHost]) {
        const rows = await autofillRows(
          native,
          connection,
          target,
          method,
          host + path,
        );
        assert(
          rows.length > 0,
          `${method} retains visible bookmarks and history`,
        );
        assert(
          rows.every((row) => row.title !== secretTitle),
          `${method} never takes the hidden duplicate's title`,
        );
      }
    }

    assertEquals(
      await native.ProviderAutofill.getTopHostOverThreshold(
        context(native, target, hiddenHost),
        [hiddenHost],
      ),
      null,
      "top-host autofill also rejects hidden-only bookmark frecency",
    );
    assertEquals(
      await native.ProviderAutofill.getTopHostOverThreshold(
        context(native, target, visibleHost),
        [visibleHost],
      ),
      visibleHost,
      "top-host autofill permits a visible bookmark",
    );
    const allItems = context(native, target, secretTitle);
    allItems.floorpWorkspaceBookmarkContext = native.scopes.getContext(target, {
      allItems: true,
    });
    assert(
      (await results(new native.ProviderPlaces(), allItems)).length > 0,
      "explicit all-items queries retain unrestricted bookmark matching",
    );

    assertEquals(
      (await results(
        new native.ProviderKeywords(),
        context(native, target, hiddenKeyword),
      )).length,
      0,
      "a URL keyword cannot expose hidden-only bookmarks",
    );
    assertEquals(
      (await results(
        new native.ProviderKeywords(),
        context(native, target, visibleKeyword),
      )).length,
      1,
      "a URL keyword remains available through a visible duplicate",
    );
    const stale = context(native, target, visibleKeyword);
    native.scopes.setWindowContext(target, otherWorkspace, true);
    assertEquals(
      (await results(new native.ProviderKeywords(), stale)).length,
      0,
      "a workspace switch prevents an earlier keyword query from emitting",
    );
  } finally {
    native.scopes.clearWindowContext(target);
    if (hiddenFolder) await native.scopes.setFolderScope(hiddenFolder, null);
    for (const keyword of [hiddenKeyword, visibleKeyword]) {
      await native.places.keywords.remove(keyword);
    }
    for (const guid of folders.reverse()) {
      await native.places.bookmarks.remove(guid);
    }
    await native.places.history.remove(urls);
    await native.scopes.refresh();
    for (const pref of oldPrefs) {
      if (pref.user) Services.prefs.setBoolPref(pref.name, pref.value);
      else Services.prefs.clearUserPref(pref.name);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkspaceBookmarkUrlbar.test.mts", [{
    name:
      "native Places, autofill, and keyword providers respect workspace scopes",
    fn: testNativeProvidersRespectWorkspaceScope,
  }]);
}
