// SPDX-License-Identifier: MPL-2.0
import type {
  BridgeCall,
  PrefValue,
  UiTestChromeHost,
  UiTestReport,
} from "./types.ts";
import {
  assert,
  type TestCase,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";

export const prefs = new Map<string, PrefValue>();
export const writes: BridgeCall[] = [];
export const calls: BridgeCall[] = [];
export const faults = { read: "", write: "" };

export function expose(name: string, value: unknown) {
  Object.defineProperty(globalThis, name, {
    configurable: true,
    writable: true,
    value,
  });
}

export function installBridge() {
  assert(
    location.pathname.endsWith("/test/integration/index.html"),
    "Mocks only run in the dedicated fixture",
  );
  assert(
    ["5196", "5197"].includes(location.port),
    "Use isolated test ports, never a native settings actor origin",
  );
  expose("Services", {
    prefs: {
      PREF_BOOL: 128,
      PREF_INT: 64,
      PREF_STRING: 32,
      getPrefType: (key: string) => {
        if (key === faults.read) throw new Error("Injected read failure");
        const type = typeof prefs.get(key);
        return type === "boolean"
          ? 128
          : type === "number"
          ? 64
          : type === "string"
          ? 32
          : 0;
      },
      getIntPref: (key: string, fallback = 1) => prefs.get(key) ?? fallback,
      getBoolPref: (key: string, fallback = false) =>
        prefs.get(key) ?? fallback,
      getStringPref: (key: string, fallback = "") => prefs.get(key) ?? fallback,
      setIntPref: writeSync,
      setBoolPref: writeSync,
      setStringPref: writeSync,
    },
    obs: {
      notifyObservers: (...args: unknown[]) =>
        calls.push({ method: "notifyObservers", args }),
    },
  });
  expose("NRI18n", {
    getOperatingSystemLocale: () => Promise.resolve("en-US"),
    getPrimaryBrowserLocaleMapped: () => Promise.resolve("en-US"),
    normalizeLocale: (locale: string) => Promise.resolve(locale),
  });
  expose(
    "NRGetAccountInfo",
    (callback: (value: string) => void) =>
      callback(JSON.stringify({ status: "not_configured" })),
  );
  expose(
    "NRGetCurrentProfile",
    (callback: (value: string) => void) =>
      callback(
        JSON.stringify({ profileName: "UI test", profilePath: "test-profile" }),
      ),
  );
  for (
    const method of [
      "NRRestartBrowser",
      "NRAddTab",
      "NRSignIn",
      "NRResetProfile",
    ]
  ) {
    expose(method, (...args: unknown[]) => calls.push({ method, args }));
  }
  expose("open", (...args: unknown[]) => {
    calls.push({ method: "open", args });
    return null;
  });
  expose("close", () => calls.push({ method: "close", args: [] }));
}

function writeSync(key: string, value: PrefValue) {
  if (key === faults.write) throw new Error("Injected write failure");
  prefs.set(key, value);
  writes.push({ method: key, args: [value] });
}

export const pause = (ms = 40) =>
  new Promise<void>((resolve) => setTimeout(resolve, ms));
export async function until(
  condition: () => unknown,
  message: string,
  timeout = 4000,
) {
  const deadline = Date.now() + timeout;
  while (!condition()) {
    if (Date.now() > deadline) throw new Error(message);
    await pause(20);
  }
}
export function element<T extends Element = HTMLElement>(selector: string): T {
  const result = document.querySelector<T>(selector);
  assert(result, `Missing element: ${selector}`);
  return result;
}
export async function click(selector: string) {
  const target = element<HTMLElement>(selector);
  assert(!target.matches(":disabled"), `Disabled: ${selector}`);
  target.click();
  await pause();
}
export async function button(text: string) {
  const scope = document.querySelector('[role="dialog"]') ?? document;
  const target = [...scope.querySelectorAll<HTMLButtonElement>("button")].find((
    item,
  ) => item.textContent?.trim() === text);
  assert(target, `Missing button: ${text}`);
  assert(!target.disabled, `Disabled button: ${text}`);
  target.click();
  await pause();
}
export async function input(selector: string, value: string) {
  const target = element<HTMLInputElement | HTMLSelectElement>(selector);
  assert(!target.matches(":disabled"), `Disabled: ${selector}`);
  target.focus();
  const prototype = target instanceof HTMLSelectElement
    ? HTMLSelectElement.prototype
    : HTMLInputElement.prototype;
  Object.getOwnPropertyDescriptor(prototype, "value")!.set!.call(target, value);
  target.dispatchEvent(new Event("input", { bubbles: true }));
  target.dispatchEvent(new Event("change", { bubbles: true }));
  await pause();
}
export function json(key: string): Record<string, unknown> {
  return JSON.parse(String(prefs.get(key)));
}
export function valueAt(key: string, path: string): unknown {
  return path.split(".").reduce<unknown>(
    (value, segment) => (value as Record<string, unknown>)?.[segment],
    json(key),
  );
}
export async function executeCases(tests: TestCase[]) {
  const report: UiTestReport = { status: "running", results: [] };
  const publish = () => {
    document.documentElement.dataset.uiTestResult = JSON.stringify(report);
  };
  publish();
  for (const test of tests) {
    document.documentElement.dataset.uiTestCurrent = test.name;
    try {
      await test.fn();
      report.results.push({ name: test.name, ok: true });
    } catch (error) {
      report.results.push({ name: test.name, ok: false, error: String(error) });
    }
    publish();
  }
  report.status = "done";
  publish();
}

// The standard colocated runner executes in browser chrome. Keep React and the
// mocked bridge inside a remote content browser. Use its message manager to
// read the report across Gecko's process boundary.
export async function runFixture(url: string) {
  const host = globalThis as unknown as UiTestChromeHost;
  const frame = host.document.createXULElement("browser");
  frame.setAttribute("type", "content");
  frame.setAttribute("remote", "true");
  frame.setAttribute(
    "style",
    "position:fixed;inset:0;width:1280px;height:900px;z-index:2147483647;background:white",
  );
  document.documentElement.append(frame);
  frame.focus();
  const request = `FloorpUiTest:read:${crypto.randomUUID()}`;
  const response = `${request}:result`;
  let serializedReport: string | undefined;
  const receive = (message: { data: string | undefined }) => {
    serializedReport = message.data;
  };
  const manager = frame.messageManager;
  manager.addMessageListener(response, receive);
  try {
    manager.loadFrameScript(
      `data:application/javascript,${
        encodeURIComponent(
          `addMessageListener(${
            JSON.stringify(request)
          }, () => sendAsyncMessage(${
            JSON.stringify(response)
          }, content.document.documentElement?.dataset.uiTestResult));`,
        )
      }`,
      false,
    );
    frame.loadURI(host.Services.io.newURI(url), {
      triggeringPrincipal: host.Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    await until(
      () => {
        manager.sendAsyncMessage(request);
        return serializedReport?.includes('"status":"done"');
      },
      "UI fixture did not finish",
      110000,
    );
    const report = JSON.parse(serializedReport!) as UiTestReport;
    const failures = report.results.filter((result) => !result.ok);
    assert(report.results.length > 0, "No UI checks executed");
    assert(
      failures.length === 0,
      failures.map((result) => `${result.name}: ${result.error}`).join("\n"),
    );
  } finally {
    manager.removeMessageListener(response, receive);
    frame.remove();
  }
}
