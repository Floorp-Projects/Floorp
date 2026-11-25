// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

type ActivityWindow = Window & {
  browsingContext: BrowsingContext & {
    readonly topChromeWindow: Window;
    forceAppWindowActive: boolean;
  };
  gBrowser: { selectedBrowser: XULBrowserElement };
};

interface PictureInPictureActivity {
  originatingWinWeakMap: WeakMap<Window, number>;
  addOriginatingWinToWeakMap(browser: XULBrowserElement): void;
  removeOriginatingWinFromWeakMap(browser: XULBrowserElement): void;
}

async function waitFor(check: () => boolean): Promise<void> {
  for (let attempt = 0; attempt < 200; attempt++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error("Timed out preparing native embedded chrome document");
}

async function testSharedAppWindow(panelFirst: boolean): Promise<void> {
  const pip = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/pictureinpicture/PictureInPicture.sys.mjs",
  ).PictureInPicture as PictureInPictureActivity;
  const open = globalThis as unknown as { OpenBrowserWindow(): ActivityWindow };
  const app = open.OpenBrowserWindow();
  let outer: XULBrowserElement | null = null;
  const registered: XULBrowserElement[] = [];
  try {
    await waitFor(() => !!app.gBrowser?.selectedBrowser);
    outer = app.document.createXULElement("browser") as XULBrowserElement;
    outer.setAttribute("src", "about:blank");
    app.document.documentElement.appendChild(outer);
    await waitFor(() => !!outer?.contentDocument?.body);
    const embedded = outer.contentWindow as ActivityWindow;
    assert(embedded !== app, "fixture has a real nested chrome window");
    assert(
      embedded.browsingContext.topChromeWindow === app,
      "nested chrome belongs to the containing app window",
    );
    const inner = app.document.createXULElement("browser") as XULBrowserElement;
    outer.contentDocument!.body.appendChild(inner);
    const normal = app.gBrowser.selectedBrowser;
    const browsers = panelFirst ? [inner, normal] : [normal, inner];

    for (const browser of browsers) {
      pip.addOriginatingWinToWeakMap(browser);
      registered.push(browser);
    }
    assertEquals(
      pip.originatingWinWeakMap.get(app),
      2,
      "one AppWindow owns both players",
    );
    assertEquals(
      app.browsingContext.forceAppWindowActive,
      true,
      "AppWindow is kept active",
    );
    assertEquals(
      embedded.browsingContext.forceAppWindowActive,
      false,
      "embedded context is not incorrectly forced as an AppWindow",
    );

    pip.removeOriginatingWinFromWeakMap(browsers[0]);
    registered.shift();
    assertEquals(
      pip.originatingWinWeakMap.get(app),
      1,
      "first close retains the other player",
    );
    assertEquals(
      app.browsingContext.forceAppWindowActive,
      true,
      "first close retains activity",
    );
    pip.removeOriginatingWinFromWeakMap(browsers[1]);
    registered.shift();
    assertEquals(
      pip.originatingWinWeakMap.has(app),
      false,
      "last close releases the count",
    );
    assertEquals(
      app.browsingContext.forceAppWindowActive,
      false,
      "last close releases activity",
    );
  } finally {
    for (const browser of registered) {
      pip.removeOriginatingWinFromWeakMap(browser);
    }
    outer?.remove();
    app.close();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("pictureInPictureActivity.test.ts", [
    {
      name: "normal then embedded player share AppWindow activity",
      fn: () => testSharedAppWindow(false),
    },
    {
      name: "embedded then normal player share AppWindow activity",
      fn: () => testSharedAppWindow(true),
    },
  ]);
}
