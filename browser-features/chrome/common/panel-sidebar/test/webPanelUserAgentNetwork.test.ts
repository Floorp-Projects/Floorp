// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarData,
} from "../data/data.ts";
import { PanelNavigator } from "../panel-navigator.ts";
import {
  getWebPanelChromeWindow,
  getWebPanelContentBrowser,
  loadUriInWebPanelBrowser,
  type WebPanelBrowserElement,
} from "../utils/web-panel-browser.ts";
import { getWebPanelWindowByBrowserId } from "../utils/webRequest.ts";

// A loopback HTTP echo server using shipped XPCOM APIs, so the colocated
// runner does not need a mochitest-only testing-common resource mapping.
async function testNetworkUserAgent(): Promise<void> {
  const mobileUA =
    "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";
  const server = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket,
  );
  const received = new Map<string, string[]>();
  const socketStates: string[] = [];
  const transports: nsISocketTransport[] = [];
  server.init(-1, true, -1);
  server.asyncListen({
    onSocketAccepted(
      _socket: nsIServerSocket,
      transport: nsISocketTransport,
    ): void {
      transports.push(transport);
      socketStates.push("accepted");
      const stream = transport.openInputStream(0, 0, 0);
      assert(stream.QueryInterface, "socket input must support QueryInterface");
      const input = stream.QueryInterface(Ci.nsIAsyncInputStream);
      const reader = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream,
      );
      reader.init(input);
      let headers = "";
      const callback = {
        onInputStreamReady(): void {
          socketStates.push("input ready");
          const available = input.available();
          if (!available) return;
          headers += reader.read(available);
          if (!headers.includes("\r\n\r\n")) {
            input.asyncWait(callback, 0, 0, Services.tm.currentThread);
            return;
          }
          const target = headers.split(" ")[1];
          socketStates.push(target);
          const ua = /^User-Agent: (.*)\r?$/im.exec(headers)?.[1]?.trim() ?? "";
          const body =
            "<!doctype html><title>User-Agent regression</title><p>Loaded</p>";
          const reply =
            `HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ${body.length}\r\n\r\n${body}`;
          const output = transport.openOutputStream(0, 0, 0);
          const written = output.write(reply, reply.length);
          socketStates.push(`replied ${written}/${reply.length}`);
          output.close();
          const key = new URL(target, "http://localhost").search.slice(1);
          received.set(key, [...received.get(key) ?? [], ua]);
        },
      };
      input.asyncWait(callback, 0, 0, Services.tm.currentThread);
    },
    onStopListening(_socket: nsIServerSocket, status: number): void {
      socketStates.push(`stopped ${status}`);
    },
  });
  const base = `http://127.0.0.1:${server.port}/ua`;
  const panels = panelSidebarData();
  const selected = selectedPanelId();
  const selectedTab = gBrowser.selectedTab;
  const controller = PanelNavigator.gPanelSidebar;
  assert(controller, "panel sidebar should be initialized");
  const id = "web-panel-ua-network-regression";
  const otherId = `${id}-desktop`;
  const ids = new Map<string, number>();
  const observer = {
    observe(subject: nsISupports): void {
      assert(
        subject.QueryInterface,
        "HTTP channel must support QueryInterface",
      );
      const channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (channel.URI.spec.startsWith(base)) {
        ids.set(new URL(channel.URI.spec).search.slice(1), channel.browserId);
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");
  let tab: ReturnType<typeof gBrowser.addTab> | undefined;
  async function waitFor(check: () => boolean, label: string): Promise<void> {
    for (let attempt = 0; attempt < 600; attempt++) {
      if (check()) return;
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    throw new Error(
      `Timed out waiting for UA network regression: ${label}; ${
        JSON.stringify({
          received: [...received],
          ids: [...ids],
          socketStates,
          chrome: !!getWebPanelChromeWindow(id),
          mobile: getWebPanelChromeWindow(id)?.floorpBmsUserAgent,
          browserId: getWebPanelContentBrowser(id)?.browserId,
          uri: getWebPanelContentBrowser(id)?.currentURI?.spec,
          loading: getWebPanelContentBrowser(id)?.webProgress
            ?.isLoadingDocument,
        })
      }`,
    );
  }
  const ua = (key: string): string => received.get(key)?.at(-1) ?? "";
  const count = (key: string): number => received.get(key)?.length ?? 0;

  async function ready(
    panelId: string,
    mobile: boolean,
  ): Promise<WebPanelBrowserElement> {
    await waitFor(
      () =>
        !!getWebPanelContentBrowser(panelId)?.browserId &&
        getWebPanelChromeWindow(panelId)?.floorpBmsUserAgent === mobile,
      `${panelId} initialization (mobile=${mobile})`,
    );
    const browser = getWebPanelContentBrowser(panelId)!;
    const key = panelId === id ? "initial-mobile" : "initial-other";
    await waitFor(
      () =>
        received.has(key) && ids.get(key) === browser.browserId &&
        ua(key).includes("Mobile") === mobile,
      `${panelId} initial HTTP request`,
    );
    if (mobile) {
      assertEquals(
        ua(key),
        mobileUA,
        "initial request must send exact mobile UA",
      );
    }
    await waitFor(
      () =>
        browser.currentURI?.spec === `${base}?${key}` &&
        !browser.webProgress?.isLoadingDocument,
      `${panelId} initial document load`,
    );
    return browser;
  }

  async function navigate(
    browser: WebPanelBrowserElement,
    key: string,
  ): Promise<void> {
    const previous = count(key);
    loadUriInWebPanelBrowser(browser, `${base}?${key}`);
    await waitFor(() => count(key) > previous, `${key} HTTP request`);
    await waitFor(() =>
      browser.currentURI?.spec === `${base}?${key}` &&
      !browser.webProgress?.isLoadingDocument, `${key} document load`);
    assertEquals(
      ids.get(key),
      browser.browserId,
      "HTTP channel must identify the inner content browser",
    );
  }

  async function reload(
    browser: WebPanelBrowserElement,
    key: string,
  ): Promise<void> {
    const previous = count(key);
    assert(browser.reload, "browser must support reload");
    browser.reload();
    await waitFor(() => count(key) > previous, `${key} reload HTTP request`);
    await waitFor(
      () => !browser.webProgress?.isLoadingDocument,
      `${key} reload completion`,
    );
    assertEquals(
      ids.get(key),
      browser.browserId,
      "reload must preserve browser identity",
    );
  }

  try {
    const panel = {
      id,
      type: "web" as const,
      width: 400,
      url: `${base}?initial-mobile`,
      userAgent: true,
      userContextId: 0,
      icon: undefined,
      zoomLevel: undefined,
      extensionId: undefined,
    };
    setPanelSidebarData([...panels, panel, {
      ...panel,
      id: otherId,
      url: `${base}?initial-other`,
      userAgent: false,
    }]);
    controller.changePanel(id);
    const mobile = await ready(id, true);
    const originalId = mobile.browserId;
    await navigate(mobile, "mobile");
    assert(ua("mobile").includes("Mobile"), "server must receive mobile UA");
    assertEquals(
      getWebPanelWindowByBrowserId(originalId),
      getWebPanelChromeWindow(id),
      "lookup must resolve the live panel",
    );
    const host = document.getElementById(`sidebar-panel-${id}`) as
      | XULBrowserElement
      | null;
    assert(host, "outer panel host must exist");
    assertEquals(
      getWebPanelWindowByBrowserId(host.browserId),
      null,
      "outer chrome host must not match the inner content browser",
    );

    await reload(mobile, "mobile");
    assert(ua("mobile").includes("Mobile"), "reload must preserve mobile UA");
    await navigate(mobile, "mobile-navigation");
    assert(
      ua("mobile-navigation").includes("Mobile"),
      "navigation must preserve mobile UA",
    );
    assertEquals(
      mobile.browserId,
      originalId,
      "navigation must preserve panel identity",
    );

    controller.changePanel(otherId);
    const other = await ready(otherId, false);
    await navigate(other, "other-panel");
    assert(
      !ua("other-panel").includes("Mobile"),
      "second panel must retain desktop UA",
    );
    await navigate(mobile, "background-mobile");
    assert(
      ua("background-mobile").includes("Mobile"),
      "hidden mobile panel must retain its own UA",
    );

    tab = gBrowser.addTab(`${base}?tab`, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    assert(tab.linkedBrowser, "normal tab must have a browser");
    gBrowser.selectedTab = tab;
    await waitFor(() => received.has("tab"), "normal tab HTTP request");
    await waitFor(
      () => !tab?.linkedBrowser?.webProgress?.isLoadingDocument,
      "normal tab load",
    );
    const desktopUA = ua("tab");
    assert(
      desktopUA.length > 0 && !desktopUA.includes("Mobile"),
      "normal tab must retain a nonempty desktop UA",
    );
    assertEquals(
      ua("other-panel"),
      desktopUA,
      "second panel must use the normal desktop UA",
    );
    assertEquals(
      getWebPanelWindowByBrowserId(tab.linkedBrowser.browserId),
      null,
      "normal tab must not match a web panel",
    );

    controller.changeUserAgent(id);
    const disabled = await ready(id, false);
    const disabledId = disabled.browserId;
    assert(
      disabled.browserId !== originalId,
      "settings toggle must recreate the content browser",
    );
    assertEquals(
      getWebPanelWindowByBrowserId(originalId),
      null,
      "retired mobile panel browser must not match",
    );
    assertEquals(
      panelSidebarData().find((panel) => panel.id === id)?.userAgent,
      false,
      "mobile OFF must persist in panel settings",
    );
    await navigate(disabled, "disabled");
    assertEquals(ua("disabled"), desktopUA, "mobile OFF must send desktop UA");
    await reload(disabled, "disabled");
    assertEquals(ua("disabled"), desktopUA, "mobile OFF must survive reload");

    controller.changeUserAgent(id);
    const enabled = await ready(id, true);
    assertEquals(
      getWebPanelWindowByBrowserId(disabledId),
      null,
      "retired desktop panel browser must not match",
    );
    await navigate(enabled, "reenabled");
    assert(
      ua("reenabled").includes("Mobile"),
      "mobile ON must restore mobile UA",
    );
    const retiredId = enabled.browserId;
    controller.deletePanel(id);
    await waitFor(
      () => getWebPanelContentBrowser(id) === null,
      "panel removal",
    );
    assertEquals(
      getWebPanelWindowByBrowserId(retiredId),
      null,
      "removed panel must not leave a browser identity behind",
    );
    await reload(tab.linkedBrowser, "tab");
    assertEquals(
      ua("tab"),
      desktopUA,
      "normal tab reload after panel removal must retain desktop UA",
    );
    await navigate(other, "other-after-removal");
    assertEquals(
      ua("other-after-removal"),
      desktopUA,
      "removing mobile panel must not affect another panel",
    );
    for (
      const key of [
        "mobile",
        "mobile-navigation",
        "background-mobile",
        "reenabled",
      ]
    ) {
      assert(
        received.has(key),
        `server must receive ${key} requests`,
      );
      for (const actualUA of received.get(key)!) {
        assertEquals(
          actualUA,
          mobileUA,
          `${key} must send the exact mobile UA`,
        );
      }
    }
  } finally {
    Services.obs.removeObserver(observer, "http-on-modify-request");
    if (tab) gBrowser.removeTab(tab);
    controller.unloadPanel(id);
    controller.unloadPanel(otherId);
    setPanelSidebarData(panels);
    gBrowser.selectedTab = selectedTab;
    if (selected) controller.changePanel(selected);
    server.close();
    for (const transport of transports) transport.close(Cr.NS_OK);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelUserAgentNetwork.test.ts", [
    {
      name:
        "HTTP UA survives toggles, navigation and reload and stays isolated through panel removal",
      fn: testNetworkUserAgent,
    },
  ]);
}
