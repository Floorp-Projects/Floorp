// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zipSync } from "fflate/browser";
import {
  assert,
  assertEquals as harnessAssertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  AuthWindow,
  AuthWindowPhase,
  Extension,
  Fixture,
  Reply,
} from "./helpers/identity-test-types.ts";

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "Native identity result matches",
): void {
  if (actual !== null && typeof actual === "object") {
    harnessAssertEquals(
      JSON.stringify(actual),
      JSON.stringify(expected),
      message,
    );
  } else harnessAssertEquals(actual, expected, message);
}

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const CHROME_ID = "d".repeat(32);
const CLIENT_ID = "floorp-loopback-transport-test.apps.googleusercontent.com";

async function waitFor<T>(
  read: () => T | false | undefined,
  description: string,
): Promise<T> {
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    const value = read();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

const background = `
  window.addEventListener('message', async event => {
    const data = event.data;
    if (data?.floorpIdentityFixture !== true) return;
    try {
      let result;
      switch (data.operation) {
        case 'inspect':
          result = { redirect: browser.identity.getRedirectURL(data.path),
            tokenType: typeof browser.identity.getAuthToken,
            clearType: typeof browser.identity.clearAllCachedAuthTokens };
          break;
        case 'launch': result = await browser.identity.launchWebAuthFlow(data.details); break;
        case 'token': result = await browser.identity.getAuthToken(data.details); break;
        case 'tokenCallback':
          result = await new Promise((resolve, reject) => {
            chrome.identity.getAuthToken(data.details, (token, grantedScopes) => {
              if (chrome.runtime.lastError) reject(new Error(chrome.runtime.lastError.message));
              else resolve({ token, grantedScopes });
            });
          });
          break;
        case 'accounts': result = await browser.identity.getAccounts(); break;
        case 'tokenCallbackError':
          result = await new Promise(resolve => {
            chrome.identity.getAuthToken(data.details, function() {
              resolve({ count: arguments.length, error: chrome.runtime.lastError?.message });
            });
          });
          break;
        case 'profile': result = await browser.identity.getProfileUserInfo({ accountStatus: 'ANY' }); break;
        case 'remove': result = await browser.identity.removeCachedAuthToken({ token: data.token }); break;
        case 'clear': result = await browser.identity.clearAllCachedAuthTokens(); break;
        case 'removeCallback':
        case 'clearCallback':
          result = await new Promise((resolve, reject) => {
            const callback = function() {
              if (chrome.runtime.lastError) reject(new Error(chrome.runtime.lastError.message));
              else resolve(arguments.length);
            };
            if (data.operation === 'removeCallback')
              chrome.identity.removeCachedAuthToken({ token: data.token }, callback);
            else chrome.identity.clearAllCachedAuthTokens(callback);
          });
          break;
      }
      document.title = JSON.stringify({ requestId: data.requestId, result });
    } catch (error) {
      document.title = JSON.stringify({ requestId: data.requestId, error: String(error) });
    }
  });
  document.title = JSON.stringify({ ready: true });
`;

let sequence = 0;
async function command(
  fixture: Fixture,
  operation: string,
  args: Record<string, unknown> = {},
): Promise<Reply> {
  const requestId = ++sequence;
  const data = { floorpIdentityFixture: true, requestId, operation, ...args };
  fixture.extension.backgroundContext!.xulBrowser.messageManager
    .loadFrameScript(
      `data:application/javascript,${
        encodeURIComponent(`content.postMessage(${JSON.stringify(data)}, '*');`)
      }`,
      false,
    );
  return await waitFor(() => {
    try {
      const value = JSON.parse(
        fixture.extension.backgroundContext!.xulBrowser.contentTitle,
      ) as Reply;
      return value.requestId === requestId ? value : false;
    } catch {
      return false;
    }
  }, `${operation} API response`);
}

async function fixture(
  fn: (fixture: Fixture) => Promise<void>,
  { compatible = true, oauth = true } = {},
): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const id = compatible
    ? `${CHROME_ID}@chromium-extension`
    : "ordinary-identity@floorp-test";
  const manifest = {
    manifest_version: 2,
    name: "Floorp native OAuth loopback fixture",
    version: "1.0",
    permissions: compatible ? ["identity", "identity.email"] : ["identity"],
    background: { scripts: ["background.js"] },
    ...(oauth
      ? { oauth2: { client_id: CLIENT_ID, scopes: ["fixture.scope"] } }
      : {}),
    browser_specific_settings: {
      gecko: {
        id,
        ...(compatible
          ? {
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: CHROME_ID,
            },
          }
          : {}),
      },
    },
  };
  const path = PathUtils.join(
    PathUtils.tempDir,
    `floorp-identity-${Services.uuid.generateUUID()}.xpi`,
  );
  const encode = (value: string) => new TextEncoder().encode(value);
  await IOUtils.write(
    path,
    zipSync({
      "manifest.json": encode(JSON.stringify(manifest)),
      "background.js": encode(background),
    }),
  );
  const addon = await AddonManager.installTemporaryAddon(
    new FileUtils.File(path),
  );
  try {
    const extensions = ExtensionParent.GlobalManager.extensionMap as Map<
      string,
      Extension
    >;
    const extension = await waitFor(
      () => extensions.get(id),
      "native identity extension",
    );
    await waitFor(
      () =>
        extension.backgroundContext?.xulBrowser.contentTitle.includes(
          '"ready":true',
        ),
      "native identity background",
    );
    await fn({ id, extension });
  } finally {
    await addon.uninstall();
    await IOUtils.remove(path);
  }
}

function fixtureURL(path: string, params: Record<string, string> = {}): string {
  const host = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as {
    wrappedJSObject: { __floorpRuntimeFixtureURL: string };
  };
  const url = new URL(path, host.wrappedJSObject.__floorpRuntimeFixtureURL);
  for (const [key, value] of Object.entries(params)) {
    url.searchParams.set(key, value);
  }
  return url.href;
}
function success(reply: Reply): unknown {
  assert(!reply.error, reply.error ?? "Native identity call succeeds");
  return reply.result;
}
function denied(reply: Reply, expected: string): void {
  assert(
    !!reply.error?.includes(expected),
    `Expected ${expected}; received ${JSON.stringify(reply)}`,
  );
}

// The native browser executes its fixed Google provider URL. A test-only
// observer redirects that request to our local HTTP provider transport. This
// tests token parsing/cache/API behavior and does not claim Google registration.
async function oauthTransport(
  fn: (counter: { count: number; mode: string }) => Promise<void>,
): Promise<void> {
  const counter = { count: 0, mode: "" };
  const observer = {
    observe(subject: nsISupports) {
      const channel = subject.QueryInterface!(Ci.nsIHttpChannel);
      const url = new URL(channel.URI.spec);
      if (
        url.origin !== "https://accounts.google.com" ||
        url.pathname !== "/o/oauth2/v2/auth" ||
        url.searchParams.get("client_id") !== CLIENT_ID
      ) {
        return;
      }
      counter.count++;
      const params = Object.fromEntries(url.searchParams);
      params.fixture_mode = counter.mode;
      channel.redirectTo(
        Services.io.newURI(fixtureURL("identity-token", params)),
      );
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");
  try {
    await fn(counter);
  } finally {
    Services.obs.removeObserver(observer, "http-on-modify-request");
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("Native Chrome identity", [
    {
      name: "redirect identities are scoped to explicit Chrome compatibility",
      fn: async () => {
        await fixture(async (extension) => {
          assertEquals(
            success(
              await command(extension, "inspect", { path: "oauth/callback" }),
            ),
            {
              redirect: `https://${CHROME_ID}.chromiumapp.org/oauth/callback`,
              tokenType: "function",
              clearType: "function",
            },
          );
        });
        await fixture(
          async (extension) => {
            const result = success(await command(extension, "inspect")) as {
              redirect: string;
              tokenType: string;
              clearType: string;
            };
            assert(
              /^https:\/\/[a-f0-9]{40}\./.test(result.redirect),
              "Ordinary Firefox redirect retains its SHA1 identity",
            );
            assertEquals(result.tokenType, "undefined");
            assertEquals(result.clearType, "undefined");
          },
          { compatible: false },
        );
      },
    },
    {
      name:
        "noninteractive OAuth captures exact approved redirects and rejects hostile origins",
      fn: () =>
        fixture(async (extension) => {
          const redirect =
            `https://${CHROME_ID}.chromiumapp.org/oauth/callback#code=local-provider`;
          assertEquals(
            success(
              await command(extension, "launch", {
                details: {
                  url: fixtureURL("identity-redirect", {
                    redirect_uri: redirect,
                  }),
                  interactive: false,
                },
              }),
            ),
            redirect,
          );
          for (
            const target of [
              `https://${CHROME_ID}.chromiumapp.org.attacker.invalid/`,
              `https://attacker@${CHROME_ID}.chromiumapp.org/`,
              `https://${"e".repeat(32)}.chromiumapp.org/`,
            ]
          ) {
            denied(
              await command(extension, "launch", {
                details: {
                  url: fixtureURL("identity-redirect", {
                    redirect_uri: target,
                  }),
                  interactive: false,
                },
              }),
              "redirect_uri not allowed",
            );
          }
          denied(
            await command(extension, "launch", {
              details: {
                url: fixtureURL("identity-no-redirect"),
                interactive: false,
              },
            }),
            "Requires user interaction",
          );
        }),
    },
    {
      name:
        "interactive OAuth uses and closes a real browser authentication window",
      fn: () =>
        fixture(async (extension) => {
          const redirect =
            `https://${CHROME_ID}.chromiumapp.org/interactive?code=local-provider`;
          assertEquals(
            success(
              await command(extension, "launch", {
                details: {
                  url: fixtureURL("identity-interactive", {
                    redirect_uri: redirect,
                  }),
                  interactive: true,
                },
              }),
            ),
            redirect,
          );
          await waitFor(
            () =>
              !Array.from(Services.wm.getEnumerator("")).some(
                (window) => window.name === "launchWebAuthFlow_dialog",
              ),
            "Completed authentication window closes",
          );
        }),
    },
    {
      name:
        "closing the real authentication window before its browser document commits rejects",
      fn: () =>
        fixture(async (extension) => {
          let authWindow: AuthWindow | undefined;
          let phase: AuthWindowPhase | undefined;
          let observedClosed = false;
          const observer = {
            observe(subject: nsISupports, topic: string) {
              const window = subject as unknown as AuthWindow;
              if (topic === "domwindowopened" && !authWindow) {
                authWindow = window;
                phase = {
                  uri: window.document.documentURI,
                  readyState: window.document.readyState,
                  initial: window.document.isUncommittedInitialDocument,
                };
                window.close();
              } else if (
                topic === "domwindowclosed" && window === authWindow
              ) {
                observedClosed = true;
              }
            },
          };
          Services.ww.registerNotification(observer);
          try {
            const request = command(extension, "launch", {
              details: { url: fixtureURL("identity-idle"), interactive: true },
            });
            const initial = await waitFor(
              () => phase,
              "real authentication window initial document",
            );
            assertEquals(initial.uri, "about:blank");
            assertEquals(initial.readyState, "complete");
            assert(
              initial.initial,
              "Close occurs before browser document commit",
            );
            await waitFor(
              () => observedClosed && authWindow?.closed,
              "actual authentication window closure",
            );
            denied(await request, "User cancelled");
          } finally {
            Services.ww.unregisterNotification(observer);
          }
        }),
    },
    {
      name:
        "closing an authentication window rejects and releases the actual pending flow",
      fn: () =>
        fixture(async (extension) => {
          const request = command(extension, "launch", {
            details: { url: fixtureURL("identity-idle"), interactive: true },
          });
          const authWindow = await waitFor(
            () =>
              Array.from(Services.wm.getEnumerator("")).find(
                (window) => window.name === "launchWebAuthFlow_dialog",
              ),
            "interactive authentication window",
          );
          await waitFor(
            () =>
              authWindow.document.documentURI ===
                "chrome://browser/content/browser.xhtml" &&
              authWindow.document.readyState === "complete",
            "committed authentication browser document loaded",
          );
          authWindow.close();
          denied(await request, "User cancelled");
        }),
    },
    {
      name:
        "registered token requests parse actual loopback redirects, share cache, and adapt Chrome callbacks",
      fn: () =>
        fixture(async (extension) => {
          await oauthTransport(async (counter) => {
            const expected = {
              token: "loopback-test-token",
              grantedScopes: ["fixture.scope"],
            };
            assertEquals(
              success(
                await command(extension, "token", {
                  details: { interactive: false },
                }),
              ),
              expected,
            );
            assertEquals(counter.count, 1);
            assertEquals(
              success(
                await command(extension, "tokenCallback", { details: {} }),
              ),
              expected,
            );
            assertEquals(
              counter.count,
              1,
              "Callback reads the same authenticated token cache",
            );
            assertEquals(
              success(
                await command(extension, "removeCallback", {
                  token: "loopback-test-token",
                }),
              ),
              0,
              "Removing a cached token invokes the Chrome callback without arguments",
            );
            assertEquals(success(await command(extension, "token")), expected);
            assertEquals(
              counter.count,
              2,
              "Removing the cached token requires another provider request",
            );
            assertEquals(success(await command(extension, "clearCallback")), 0);
            assertEquals(success(await command(extension, "accounts")), []);
            assertEquals(success(await command(extension, "profile")), {
              email: "",
              id: "",
            });
            assertEquals(success(await command(extension, "token")), expected);
            assertEquals(
              counter.count,
              3,
              "Clearing browser authorization requires another provider request",
            );
          });
        }),
    },
    {
      name:
        "OAuth state, denied authorization, and missing grants fail without caching a token",
      fn: () =>
        fixture(async (extension) => {
          await oauthTransport(async (counter) => {
            for (
              const [mode, expected] of [
                ["bad-state", "state validation"],
                ["denied", "access_denied"],
                ["missing-scope", "did not grant"],
              ]
            ) {
              counter.mode = mode;
              denied(await command(extension, "token"), expected);
            }
            assertEquals(counter.count, 3);
            counter.mode = "";
            success(await command(extension, "token"));
            assertEquals(
              counter.count,
              4,
              "Rejected provider responses do not populate the token cache",
            );
          });
        }),
    },
    {
      name:
        "unregistered OAuth clients never receive synthetic browser authorization",
      fn: () =>
        fixture(
          async (extension) => {
            denied(
              await command(extension, "token"),
              "registered OAuth client_id",
            );
            const callback = success(
              await command(extension, "tokenCallbackError"),
            ) as {
              count: number;
              error: string;
            };
            assertEquals(callback.count, 0);
            assert(
              callback.error.includes("registered OAuth client_id"),
              "Failed Chrome callbacks expose the real OAuth error through lastError",
            );
            assertEquals(success(await command(extension, "accounts")), []);
            assertEquals(success(await command(extension, "profile")), {
              email: "",
              id: "",
            });
          },
          { oauth: false },
        ),
    },
  ]);
}
