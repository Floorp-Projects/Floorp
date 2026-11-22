// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act } from "react";
import { createRoot } from "react-dom/client";
import { createInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import {
  MemoryRouter,
  Route,
  Routes,
  useLocation,
  useNavigate,
} from "react-router-dom";
import { Home, Puzzle } from "lucide-react";
import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  assertStringArrayEqual,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import { SidebarProvider } from "../../src/components/common/sidebar.tsx";
import {
  type Feature,
  NavFeatures,
  NavigationFocusScope,
} from "../../src/components/nav-features.tsx";
import {
  featureHubNavigation,
  FeatureHubProvider,
  FeatureHubViewBoundary,
  HubExtensionPage,
  useFeatureHub,
} from "../../src/lib/feature-hub/FeatureHubProvider.tsx";

function assert(
  condition: unknown,
  message = "assertion failed",
): asserts condition {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "values should match",
): void {
  harnessAssertEquals(actual, expected, message);
}

interface HubSnapshotEntry {
  featureId: string;
  ownerHostEpoch: string;
  ownerGeneration: number;
  localId: string;
  route: string;
  titleKey: string;
  navigation: { labelKey: string; order: number };
  view: {
    kind: "message";
    titleKey: string;
    bodyKey: string;
    params?: Record<string, string>;
  };
}

function sampleEntry(
  overrides: Partial<HubSnapshotEntry> = {},
): HubSnapshotEntry {
  return {
    featureId: "sample",
    ownerHostEpoch: "host-one",
    ownerGeneration: 1,
    localId: "home",
    route: "/extensions/sample/home",
    titleKey: "hub.title",
    navigation: { labelKey: "hub.navigation", order: 1 },
    view: {
      kind: "message",
      titleKey: "hub.title",
      bodyKey: "hub.body",
      params: { name: "<img src=x onerror=alert(1)>" },
    },
    ...overrides,
  };
}

function snapshotEvent(
  requestId: string,
  nonce: string,
  epoch: string,
  hubRevision: number,
  entries: HubSnapshotEntry[],
): CustomEvent<string> {
  return new CustomEvent("nora:feature-hub-snapshot", {
    detail: JSON.stringify({
      type: "snapshot",
      requestId,
      connectionNonce: nonce,
      snapshot: { epoch, hubRevision, entries },
    }),
  });
}

async function flushReact(): Promise<void> {
  await act(async () => {
    await Promise.resolve();
    await Promise.resolve();
  });
}

function ThrowingDynamicView(): never {
  throw new Error("intentional dynamic render failure");
}

async function renderHub(initialRoute: string, throwDynamicView = false) {
  const i18n = createInstance();
  await i18n.init({
    lng: "en-US",
    fallbackLng: ["ja-JP"],
    defaultNS: "translations",
    ns: ["translations"],
    initImmediate: false,
    interpolation: { escapeValue: false },
    resources: {
      "en-US": {
        translations: {
          "featureHub.safeUnavailable": "Feature content is unavailable.",
          "featureHub.unavailableTitle": "Extension unavailable",
          "featureHub.unavailableBody": "This page is no longer available.",
          "hub.title": "Welcome",
          "hub.body": "Rendered for {{name}}",
          "hub.navigation": "Sample extension",
          "hub.secondNavigation": "Second extension",
          "hub.object": { nested: "Object value" },
          "hub.array": ["Array value"],
          "scope:leaf": "Namespaced-looking key",
          "hub.param": "Value: {{value}}",
        },
      },
      "ja-JP": {
        translations: {
          "hub.array": "Array fallback must not render",
          "hub.fallbackOnly": "Fallback language value",
          "hub.object": "Object fallback must not render",
        },
      },
    },
  });
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  let pathname = "";
  let requestId = "";
  const onConnect = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") requestId = detail;
    event.stopImmediatePropagation();
  };
  const isolateActorTransport = (event: Event) =>
    event.stopImmediatePropagation();
  globalThis.addEventListener("nora:feature-hub-connect", onConnect, true);
  globalThis.addEventListener(
    "nora:feature-hub-ack",
    isolateActorTransport,
    true,
  );
  globalThis.addEventListener(
    "nora:feature-hub-disconnect",
    isolateActorTransport,
    true,
  );
  let featureKeys: string[] = [];
  let translate: ReturnType<typeof useFeatureHub>["translate"] | null = null;
  let navigate: ReturnType<typeof useNavigate> | null = null;
  function Page() {
    const hub = useFeatureHub();
    translate = hub.translate;
    pathname = useLocation().pathname;
    navigate = useNavigate();
    const navigation = featureHubNavigation(
      hub.snapshot,
      hub.translate,
    );
    featureKeys = navigation.map((item) => item.key);
    const features: Feature[] = navigation.map((item) => ({
      reactKey: item.key,
      title: item.title,
      url: item.url,
      icon: Puzzle,
      exactMatch: true,
      focusKey: item.url,
      disabled: hub.status !== "ready",
    }));
    return (
      <>
        <SidebarProvider>
          <NavigationFocusScope>
            <NavFeatures
              title="Built-ins"
              features={[{ title: "Home", url: "/overview/home", icon: Home }]}
            />
            {features.length > 0 && (
              <NavFeatures title="Extensions" features={features} />
            )}
          </NavigationFocusScope>
        </SidebarProvider>
        <main>
          <button id="outside-focus-target" type="button" />
          <Routes>
            <Route path="/overview/home" element={<p>Built-in settings</p>} />
            <Route
              path="/extensions/:featureId/:localId"
              element={
                <FeatureHubViewBoundary>
                  {throwDynamicView
                    ? <ThrowingDynamicView />
                    : <HubExtensionPage />}
                </FeatureHubViewBoundary>
              }
            />
          </Routes>
        </main>
      </>
    );
  }
  await act(async () => {
    root.render(
      <I18nextProvider i18n={i18n}>
        <MemoryRouter initialEntries={[initialRoute]}>
          <FeatureHubProvider>
            <Page />
          </FeatureHubProvider>
        </MemoryRouter>
      </I18nextProvider>,
    );
    await Promise.resolve();
  });
  await flushReact();
  return {
    host,
    pathname: () => pathname,
    requestId: () => requestId,
    featureKeys: () => featureKeys,
    translate: (
      key: string,
      params?: Record<string, string | number | boolean>,
    ) => {
      assert(translate !== null, "the provider translation helper is mounted");
      return translate(key, params);
    },
    injectThrowingResourceLookup: () => {
      const resourceBundle = i18n.getResourceBundle(
        "en-US",
        "translations",
      ) as Record<string, unknown>;
      const originalDescriptor = Object.getOwnPropertyDescriptor(
        resourceBundle,
        "hub.param",
      );
      let getterWasRead = false;
      Object.defineProperty(resourceBundle, "hub.param", {
        configurable: true,
        enumerable: originalDescriptor?.enumerable ?? true,
        get() {
          getterWasRead = true;
          throw new Error("lookup failure");
        },
      });
      return {
        wasRead: () => getterWasRead,
        restore: () => {
          if (originalDescriptor) {
            Object.defineProperty(
              resourceBundle,
              "hub.param",
              originalDescriptor,
            );
          } else {
            delete resourceBundle["hub.param"];
          }
        },
      };
    },
    navigate: async (route: string) => {
      assert(navigate !== null, "the router has mounted");
      await act(async () => {
        await navigate!(route);
      });
      await flushReact();
    },
    cleanup: () => {
      act(() => root.unmount());
      globalThis.removeEventListener(
        "nora:feature-hub-connect",
        onConnect,
        true,
      );
      globalThis.removeEventListener(
        "nora:feature-hub-ack",
        isolateActorTransport,
        true,
      );
      globalThis.removeEventListener(
        "nora:feature-hub-disconnect",
        isolateActorTransport,
        true,
      );
      host.remove();
    },
  };
}

async function testDynamicNavigationFocusAndMatching(): Promise<void> {
  const view = await renderHub("/overview/home");
  const dynamicHref = "/extensions/sample/home";
  const getDynamicLink = (): HTMLAnchorElement => {
    const link = view.host.querySelector<HTMLAnchorElement>(
      `a[href="${dynamicHref}"]`,
    );
    assert(link, "the dynamic item is a link while the HUB is ready");
    return link;
  };
  const getOverviewLink = (): HTMLAnchorElement => {
    const link = view.host.querySelector<HTMLAnchorElement>(
      'a[href="/overview/home"]',
    );
    assert(link, "the built-in item remains a link");
    return link;
  };
  const publishSnapshot = async (
    nonce: string,
    revision: number,
    entries: HubSnapshotEntry[] = [sampleEntry()],
  ) => {
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(
          view.requestId(),
          nonce,
          "navigation-epoch",
          revision,
          entries,
        ),
      );
      await Promise.resolve();
    });
    await flushReact();
  };
  const beginResync = async () => {
    await act(async () => {
      globalThis.dispatchEvent(new Event("nora:feature-hub-resyncing"));
      await Promise.resolve();
    });
    await flushReact();
  };

  try {
    await publishSnapshot("navigation-nonce-one", 1);
    await view.navigate(dynamicHref);
    let dynamicLink = getDynamicLink();
    assertEquals(dynamicLink.getAttribute("aria-current"), "page");
    dynamicLink.focus();
    assertEquals(document.activeElement, dynamicLink);

    await beginResync();
    const disabledItem = view.host.querySelector<HTMLElement>(
      '[aria-disabled="true"]',
    );
    assert(disabledItem, "the dynamic item is rendered disabled during resync");
    assertEquals(disabledItem.localName, "div");
    assertEquals(disabledItem.getAttribute("aria-disabled"), "true");
    assertEquals(disabledItem.tabIndex, -1);
    assertEquals(disabledItem.getAttribute("href"), null);
    assertEquals(disabledItem.getAttribute("role"), null);
    assertEquals(document.activeElement, disabledItem);
    const disabledPath = view.pathname();
    disabledItem.click();
    await flushReact();
    assertEquals(view.pathname(), disabledPath);

    await publishSnapshot("navigation-nonce-two", 2);
    dynamicLink = getDynamicLink();
    assertEquals(document.activeElement, dynamicLink);

    const overviewLink = getOverviewLink();
    overviewLink.focus();
    assertEquals(document.activeElement, overviewLink);
    await beginResync();
    assert(
      view.host.querySelector('[aria-disabled="true"]'),
      "the dynamic item becomes disabled again",
    );
    assertEquals(document.activeElement, overviewLink);
    await publishSnapshot("navigation-nonce-three", 3);
    assertEquals(document.activeElement, overviewLink);

    await view.navigate(`${dynamicHref}/`);
    assertEquals(getDynamicLink().getAttribute("aria-current"), null);
    await view.navigate("/extensions/sample/%68ome");
    assertEquals(getDynamicLink().getAttribute("aria-current"), null);
    await view.navigate(`${dynamicHref}/nested`);
    assertEquals(getDynamicLink().getAttribute("aria-current"), null);

    await view.navigate("/overview/home/nested");
    assertEquals(getOverviewLink().getAttribute("aria-current"), "page");
    getOverviewLink().click();
    await flushReact();
    assertEquals(view.pathname(), "/overview/home");

    const remainingEntry = sampleEntry({
      featureId: "other",
      ownerHostEpoch: "other-host",
      localId: "remaining",
      route: "/extensions/other/remaining",
      navigation: { labelKey: "hub.secondNavigation", order: 2 },
    });
    await publishSnapshot("navigation-nonce-three", 4, [
      sampleEntry(),
      remainingEntry,
    ]);
    await view.navigate(dynamicHref);
    getDynamicLink().focus();
    await publishSnapshot("navigation-nonce-three", 5, [remainingEntry]);
    const remainingLink = view.host.querySelector(
      'a[href="/extensions/other/remaining"]',
    );
    assert(
      remainingLink,
      `another dynamic descriptor remains after removal; keys=${
        JSON.stringify(view.featureKeys())
      }; links=${
        JSON.stringify(
          Array.from(view.host.querySelectorAll<HTMLAnchorElement>("a")).map(
            (link) => link.getAttribute("href"),
          ),
        )
      }`,
    );
    assertEquals(
      view.pathname(),
      dynamicHref,
      "partial descriptor removal leaves the dynamic pathname unchanged",
    );
    assertEquals(
      view.host.querySelector("#hub-unavailable-title")?.textContent,
      "Extension unavailable",
      "the removed route renders its unavailable view while another descriptor remains",
    );
    assertEquals(
      document.activeElement,
      getOverviewLink(),
      "removing the focused descriptor falls back to built-in Home",
    );

    await publishSnapshot("navigation-nonce-three", 6);
    await view.navigate(dynamicHref);
    const lastDynamicLink = getDynamicLink();
    lastDynamicLink.focus();
    await publishSnapshot("navigation-nonce-three", 7, []);
    assertEquals(
      view.host.querySelector(`a[href="${dynamicHref}"]`),
      null,
      "the last dynamic group can disappear",
    );
    assertEquals(
      document.activeElement,
      getOverviewLink(),
      "the mounted focus scope falls back after the last descriptor disappears",
    );
    assertEquals(
      view.pathname(),
      dynamicHref,
      "last descriptor removal leaves the dynamic pathname unchanged",
    );
    assertEquals(
      view.host.querySelector("#hub-unavailable-title")?.textContent,
      "Extension unavailable",
      "the last removed route renders its unavailable view",
    );

    await publishSnapshot("navigation-nonce-three", 8);
    await view.navigate(dynamicHref);
    const oldGenerationLink = getDynamicLink();
    oldGenerationLink.focus();
    await publishSnapshot("navigation-nonce-three", 9, [
      sampleEntry({ ownerGeneration: 2 }),
    ]);
    const newGenerationLink = getDynamicLink();
    assert(newGenerationLink !== oldGenerationLink);
    assertEquals(
      document.activeElement,
      newGenerationLink,
      "same-route generation replacement retains focus",
    );

    const generationReplacementLink = getDynamicLink();
    generationReplacementLink.focus();
    await publishSnapshot("navigation-nonce-three", 10, [
      sampleEntry({ ownerHostEpoch: "host-two", ownerGeneration: 2 }),
    ]);
    const hostReplacementLink = getDynamicLink();
    assert(hostReplacementLink !== generationReplacementLink);
    assertEquals(
      view.pathname(),
      dynamicHref,
      "owner-host replacement leaves the dynamic pathname unchanged",
    );
    assertEquals(
      document.activeElement,
      hostReplacementLink,
      "same-route owner-host replacement retains focus",
    );

    const overviewLinkAfterReplacement = getOverviewLink();
    overviewLinkAfterReplacement.focus();
    await publishSnapshot("navigation-nonce-three", 11, []);
    assertEquals(
      document.activeElement,
      overviewLinkAfterReplacement,
      "descriptor removal does not steal focus from a built-in link",
    );

    await publishSnapshot("navigation-nonce-three", 12);
    await view.navigate(dynamicHref);
    const outsideTarget = view.host.querySelector<HTMLButtonElement>(
      "#outside-focus-target",
    );
    assert(outsideTarget);
    outsideTarget.focus();
    await publishSnapshot("navigation-nonce-three", 13, []);
    assertEquals(
      document.activeElement,
      outsideTarget,
      "descriptor removal does not steal focus from outside the scope",
    );
  } finally {
    view.cleanup();
  }
}

async function testRenderedSnapshotAcknowledgesAfterCommitAndFreezesNavigation(): Promise<
  void
> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    assert(view !== null, "the first HUB test view mounts");
    const activeRequestId = view.requestId();
    const entry = sampleEntry();
    const second = sampleEntry({
      featureId: "sample-two",
      ownerHostEpoch: "host-two",
      localId: "details",
      route: "/extensions/sample-two/details",
      navigation: { labelKey: "hub.secondNavigation", order: 2 },
    });
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(activeRequestId, "nonce-one", "epoch-one", 1, [
          second,
          entry,
        ]),
      );
      await Promise.resolve();
    });
    await flushReact();

    assertEquals(view.pathname(), "/extensions/sample/home");
    assertEquals(
      view.host.querySelector("#hub-message-title")?.textContent,
      "Welcome",
    );
    assert(
      view.host.textContent?.includes("<img src=x onerror=alert(1)>"),
      "feature interpolation remains text",
    );
    assertEquals(view.host.querySelector("main img"), null);
    const links = [...view.host.querySelectorAll<HTMLAnchorElement>(
      "a[href^='/extensions/']",
    )];
    assertStringArrayEqual(links.map((link) => link.textContent ?? ""), [
      "Sample extension",
      "Second extension",
    ], "dynamic navigation labels are ordered");
    assertStringArrayEqual(view.featureKeys(), [
      "host-one:1:sample:home",
      "host-two:1:sample-two:details",
    ], "dynamic navigation keys include owner generation");
    assertEquals(acknowledgements.length, 1);
    const ack = JSON.parse(acknowledgements[0]) as Record<string, unknown>;
    assertEquals(ack.connectionNonce, "nonce-one");
    assertEquals(ack.requestId, activeRequestId);
    assertEquals(ack.epoch, "epoch-one");
    assertEquals(ack.hubRevision, 1);

    act(() => {
      globalThis.dispatchEvent(new Event("nora:feature-hub-disconnected"));
    });
    await flushReact();
    const dynamicLink = view.host.querySelector<HTMLElement>(
      "[aria-disabled='true']",
    );
    const builtInLink = view.host.querySelector<HTMLAnchorElement>(
      "a[href='/overview/home']",
    );
    assert(dynamicLink !== null && builtInLink !== null);
    assertEquals(dynamicLink.getAttribute("aria-disabled"), "true");
    assertEquals(dynamicLink.getAttribute("tabindex"), "-1");
    assertEquals(dynamicLink.closest("a"), null);
    assertEquals(dynamicLink.getAttribute("href"), null);
    assertEquals(builtInLink.getAttribute("aria-disabled"), null);
    assertEquals(builtInLink.getAttribute("tabindex"), null);
    act(() =>
      dynamicLink.dispatchEvent(
        new MouseEvent("click", {
          bubbles: true,
          cancelable: true,
        }),
      )
    );
    assertEquals(view.pathname(), "/extensions/sample/home");

    await view.navigate("/overview/home");
    assertEquals(
      view.host.querySelector("main")?.textContent,
      "Built-in settings",
    );
    assertEquals(view.pathname(), "/overview/home");
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

async function testRemovedRouteAcknowledgesLocalizedUnavailableCommit(): Promise<
  void
> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    assert(view !== null, "the removal test view mounts");
    const activeRequestId = view.requestId();
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(activeRequestId, "nonce-two", "epoch-two", 1, [
          sampleEntry(),
        ]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 1);

    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(activeRequestId, "nonce-two", "epoch-two", 2, []),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(view.pathname(), "/extensions/sample/home");
    assertEquals(
      view.host.querySelector("#hub-unavailable-title")?.textContent,
      "Extension unavailable",
    );
    assert(
      view.host.textContent?.includes("This page is no longer available."),
    );
    assertEquals(acknowledgements.length, 2);
    assertEquals(
      JSON.parse(acknowledgements[1]).hubRevision,
      2,
    );
    assertEquals(
      view.host.querySelector("a[href='/extensions/sample/home']"),
      null,
    );
    assert(view.host.querySelector("a[href='/overview/home']"));
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

async function testNoncanonicalAliasesRenderUnavailableAndAcknowledgeAfterCommit(): Promise<
  void
> {
  const aliases = [
    "/extensions/sample/home/",
    "/extensions/%73ample/home",
    "/Extensions/sample/home",
  ];
  for (const [index, route] of aliases.entries()) {
    const acknowledgements: string[] = [];
    const unavailableAtAck: (string | null)[] = [];
    let view: Awaited<ReturnType<typeof renderHub>> | null = null;
    const onAck = (event: Event) => {
      const detail = (event as CustomEvent<unknown>).detail;
      if (typeof detail !== "string") return;
      acknowledgements.push(detail);
      unavailableAtAck.push(
        view?.host.querySelector("#hub-unavailable-title")?.textContent ??
          null,
      );
    };
    globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
    try {
      view = await renderHub(route);
      await act(async () => {
        globalThis.dispatchEvent(
          snapshotEvent(
            view!.requestId(),
            `nonce-alias-${index}`,
            `epoch-alias-${index}`,
            1,
            [sampleEntry()],
          ),
        );
        await Promise.resolve();
      });
      await flushReact();
      assertEquals(view.pathname(), route);
      assertEquals(view.host.querySelector("#hub-message-title"), null);
      assertEquals(
        view.host.querySelector("#hub-unavailable-title")?.textContent,
        "Extension unavailable",
      );
      assert(
        view.host.textContent?.includes("This page is no longer available."),
      );
      assertEquals(acknowledgements.length, 1);
      assertEquals(JSON.parse(acknowledgements[0]).hubRevision, 1);
      assertEquals(unavailableAtAck[0], "Extension unavailable");
    } finally {
      view?.cleanup();
      globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
    }
  }
}

async function testDynamicRenderFailureWithholdsAcknowledgement(): Promise<
  void
> {
  for (
    const route of [
      "/extensions/sample/home",
      "/extensions/sample/home/",
    ]
  ) {
    const acknowledgements: string[] = [];
    const onAck = (event: Event) => {
      const detail = (event as CustomEvent<unknown>).detail;
      if (typeof detail === "string") acknowledgements.push(detail);
    };
    globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
    let view: Awaited<ReturnType<typeof renderHub>> | null = null;
    try {
      view = await renderHub(route, true);
      await act(async () => {
        globalThis.dispatchEvent(
          snapshotEvent(
            view!.requestId(),
            "nonce-render-failure",
            "epoch-render-failure",
            1,
            [sampleEntry()],
          ),
        );
        await Promise.resolve();
      });
      await flushReact();
      assertEquals(
        view.host.querySelector("[role='alert']")?.textContent,
        "Feature content is unavailable.",
      );
      assertEquals(view.host.querySelector("#hub-message-title"), null);
      assertEquals(acknowledgements.length, 0);
    } finally {
      view?.cleanup();
      globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
    }
  }
}

async function testEntryTranslationFailureWithholdsAcknowledgement(): Promise<
  void
> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    assert(view !== null, "the translation failure test view mounts");
    const activeRequestId = view.requestId();
    const untranslated = sampleEntry({
      view: {
        kind: "message",
        titleKey: "hub.missingTitle",
        bodyKey: "hub.missingBody",
      },
    });
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(activeRequestId, "nonce-three", "epoch-three", 1, [
          untranslated,
        ]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(
      view.host.querySelector("#hub-unavailable-title")?.textContent,
      "Extension unavailable",
    );
    assert(!view.host.textContent?.includes("hub.missingTitle"));
    assert(!view.host.textContent?.includes("hub.missingBody"));
    assertEquals(acknowledgements.length, 0);
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

async function testStaleRequestNonceEpochAndRevisionAreIgnored(): Promise<
  void
> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    assert(view !== null, "the stale snapshot test view mounts");
    const firstRequest = view.requestId();
    const acceptedRequest = firstRequest;
    const entry = sampleEntry();
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(firstRequest, "nonce-four", "epoch-four", 4, [entry]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 1);

    const changedSameRevision = sampleEntry({
      view: {
        kind: "message",
        titleKey: "hub.secondNavigation",
        bodyKey: "hub.body",
      },
    });
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-four", "epoch-four", 4, [
          changedSameRevision,
        ]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(
      view.host.querySelector("#hub-message-title")?.textContent,
      "Welcome",
    );
    assertEquals(acknowledgements.length, 1);
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-four", "epoch-four", 4, [entry]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 1);

    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent("old-request", "nonce-old", "epoch-old", 99, []),
      );
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-old", "epoch-new", 99, []),
      );
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-four", "epoch-four", 3, []),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(
      view.host.querySelector("#hub-message-title")?.textContent,
      "Welcome",
    );
    assertEquals(acknowledgements.length, 1);

    act(() =>
      globalThis.dispatchEvent(
        new Event("nora:feature-hub-disconnected"),
      )
    );
    await flushReact();
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-four", "epoch-four", 5, [entry]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 1);
    assertEquals(
      view.host.querySelector("[aria-disabled='true']") !== null,
      true,
    );

    act(() =>
      globalThis.dispatchEvent(
        new Event("nora:feature-hub-resyncing"),
      )
    );
    await flushReact();
    const resyncRequest = view.requestId();
    assert(resyncRequest !== acceptedRequest);
    assertEquals(
      view.host.querySelector("[aria-disabled='true']") !== null,
      true,
    );
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(acceptedRequest, "nonce-four", "epoch-four", 5, [entry]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 1);
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(resyncRequest, "nonce-five", "epoch-five", 1, [entry]),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 2);
    assertEquals(JSON.parse(acknowledgements[1]).requestId, resyncRequest);
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

async function testTranslationLeavesAndNamespaceAreSafe(): Promise<void> {
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/overview/home");
    assert(view !== null, "the translation test view mounts");
    const objectTranslation = view.translate("hub.object");
    assert(
      !objectTranslation.found,
      "object resource leaves are not translated",
    );
    assertEquals(objectTranslation.value, "Feature content is unavailable.");
    const arrayTranslation = view.translate("hub.array");
    assert(
      !arrayTranslation.found,
      "array resource leaves are not translated",
    );
    assertEquals(arrayTranslation.value, "Feature content is unavailable.");
    assertEquals(
      view.translate("hub.fallbackOnly").value,
      "Fallback language value",
      "a missing key falls back to a later language",
    );
    assertEquals(
      view.translate("scope:leaf").value,
      "Namespaced-looking key",
    );
    assertEquals(
      view.translate("hub.param", {
        value: "<img src=x onerror=alert(1)>",
        ns: "other",
        defaultValue: "raw",
      }).value,
      "Value: <img src=x onerror=alert(1)>",
    );
    const injectedLookup = view.injectThrowingResourceLookup();
    try {
      const failedTranslation = view.translate("hub.param");
      assert(!failedTranslation.found, "lookup exceptions fail closed");
      assertEquals(
        failedTranslation.value,
        "Feature content is unavailable.",
      );
      assert(injectedLookup.wasRead(), "the throwing resource leaf was read");
      assert(!view.host.textContent?.includes("hub.param"));
    } finally {
      injectedLookup.restore();
    }
  } finally {
    view?.cleanup();
  }
}

async function testUnmountCancelsRequestBeforeNonceArrives(): Promise<void> {
  const disconnects: Array<{
    requestId: string;
    connectionNonce: string | null;
  }> = [];
  const onDisconnect = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") {
      disconnects.push(JSON.parse(detail) as {
        requestId: string;
        connectionNonce: string | null;
      });
    }
  };
  globalThis.addEventListener(
    "nora:feature-hub-disconnect",
    onDisconnect,
    true,
  );
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/overview/home");
    const activeRequestId = view.requestId();
    view.cleanup();
    view = null;
    assertEquals(disconnects[0]?.requestId, activeRequestId);
    assertEquals(disconnects[0]?.connectionNonce, null);
  } finally {
    view?.cleanup();
    globalThis.removeEventListener(
      "nora:feature-hub-disconnect",
      onDisconnect,
      true,
    );
  }
}

async function testPageUsesCoreDescriptorLimits(): Promise<void> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    const activeRequestId = view.requestId();
    const overlong = sampleEntry({
      featureId: `a${"b".repeat(48)}`,
      route: `/extensions/a${"b".repeat(48)}/home`,
    });
    const tooManyParams = sampleEntry({
      view: {
        kind: "message",
        titleKey: "hub.title",
        bodyKey: "hub.body",
        params: Object.fromEntries(
          Array.from({ length: 9 }, (_, index) => [`p${index}`, "x"]),
        ),
      },
    });
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(
          activeRequestId,
          "nonce-limits",
          "epoch-limits",
          1,
          [overlong],
        ),
      );
      globalThis.dispatchEvent(
        snapshotEvent(
          activeRequestId,
          "nonce-limits",
          "epoch-limits",
          1,
          [tooManyParams],
        ),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 0);
    assertEquals(view.featureKeys().length, 0);
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

async function testMissingNavigationLabelWithholdsAcknowledgement(): Promise<
  void
> {
  const acknowledgements: string[] = [];
  const onAck = (event: Event) => {
    const detail = (event as CustomEvent<unknown>).detail;
    if (typeof detail === "string") acknowledgements.push(detail);
  };
  globalThis.addEventListener("nora:feature-hub-ack", onAck, true);
  let view: Awaited<ReturnType<typeof renderHub>> | null = null;
  try {
    view = await renderHub("/extensions/sample/home");
    const entry = sampleEntry({
      navigation: { labelKey: "hub.missingNavigation", order: 1 },
    });
    await act(async () => {
      globalThis.dispatchEvent(
        snapshotEvent(
          view!.requestId(),
          "nonce-missing-nav",
          "epoch-missing-nav",
          1,
          [entry],
        ),
      );
      await Promise.resolve();
    });
    await flushReact();
    assertEquals(acknowledgements.length, 0);
    assertEquals(
      view.host.querySelector("#hub-message-title")?.textContent,
      "Welcome",
    );
  } finally {
    view?.cleanup();
    globalThis.removeEventListener("nora:feature-hub-ack", onAck, true);
  }
}

const tests: TestCase[] = [
  {
    name:
      "dynamic navigation transfers focus across readiness, stays inert while disabled, and matches exact paths",
    fn: testDynamicNavigationFocusAndMatching,
  },
  {
    name:
      "Feature HUB snapshots render safely and acknowledge only after dynamic commit",
    fn: testRenderedSnapshotAcknowledgesAfterCommitAndFreezesNavigation,
  },
  {
    name:
      "removed HUB route acknowledges only after localized unavailable commit",
    fn: testRemovedRouteAcknowledgesLocalizedUnavailableCommit,
  },
  {
    name:
      "noncanonical React Router aliases render unavailable and acknowledge after commit",
    fn: testNoncanonicalAliasesRenderUnavailableAndAcknowledgeAfterCommit,
  },
  {
    name:
      "canonical and trailing-slash dynamic render failures withhold acknowledgement",
    fn: testDynamicRenderFailureWithholdsAcknowledgement,
  },
  {
    name:
      "entry-present HUB translation failures remain unavailable and unacknowledged",
    fn: testEntryTranslationFailureWithholdsAcknowledgement,
  },
  {
    name: "stale request, connection, epoch and revision snapshots are ignored",
    fn: testStaleRequestNonceEpochAndRevisionAreIgnored,
  },
  {
    name: "translation leaves, fixed namespace and interpolation data are safe",
    fn: testTranslationLeavesAndNamespaceAreSafe,
  },
  {
    name: "unmount cancels a request before its nonce arrives",
    fn: testUnmountCancelsRequestBeforeNonceArrives,
  },
  {
    name: "page snapshot parser enforces the shared descriptor limits",
    fn: testPageUsesCoreDescriptorLimits,
  },
  {
    name: "missing dynamic navigation labels withhold snapshot acknowledgement",
    fn: testMissingNavigationLabelWithholdsAcknowledgement,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("feature-hub-provider.test.tsx", tests);
}
