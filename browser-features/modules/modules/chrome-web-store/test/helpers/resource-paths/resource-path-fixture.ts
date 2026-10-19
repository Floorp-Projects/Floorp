// SPDX-License-Identifier: MPL-2.0
import { zipSync } from "fflate/browser";
import type {
  Envelope,
  OracleParent,
  OracleResult,
  OracleWindow,
} from "./resource-path-types.ts";
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
export async function waitFor<T>(
  read: () =>
    | T
    | false
    | null
    | undefined
    | Promise<T | false | null | undefined>,
  label: string,
): Promise<NonNullable<T>> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    const v = await read();
    if (v) return v as NonNullable<T>;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("Timed out: " + label);
}
export async function install(files: Record<string, string>, id: string) {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  const { FileUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/FileUtils.sys.mjs",
  );
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as { ExtensionParent: OracleParent };
  const path = PathUtils.join(
    PathUtils.tempDir,
    "resource-path-" + Services.uuid.generateUUID() + ".xpi",
  );
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as OracleWindow;
  const original = new Set(win.gBrowser.tabs);
  let addon: { uninstall(): Promise<void> } | undefined;
  try {
    await IOUtils.write(
      path,
      zipSync(
        Object.fromEntries(
          Object.entries(files).map((
            [key, value],
          ) => [key, new TextEncoder().encode(value)]),
        ),
      ),
    );
    addon = await AddonManager.installTemporaryAddon(new FileUtils.File(path));
    const extension = await waitFor(
      () => ExtensionParent.GlobalManager.extensionMap.get(id),
      "authored extension",
    );
    let serial = 0, closed = false;
    return {
      extension,
      win,
      async control(relative: string) {
        const tab = win.gBrowser.addTab(extension.baseURI.resolve(relative), {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        await waitFor(
          () => tab.linkedBrowser.contentTitle === "owned-resource-ready",
          "public controller",
        );
        return async (data: Record<string, unknown>): Promise<OracleResult> => {
          const current = ++serial;
          tab.linkedBrowser.messageManager.loadFrameScript(
            "data:application/javascript," +
              encodeURIComponent(
                `content.postMessage(${
                  JSON.stringify({
                    ...data,
                    resourceOracle: true,
                    serial: current,
                  })
                },"*");`,
              ),
            false,
          );
          return await waitFor(() => {
            const title = tab.linkedBrowser.contentTitle;
            if (!title.startsWith("{")) return false;
            const e = JSON.parse(title) as Envelope;
            if (e.serial !== current) return false;
            if (e.error) throw new Error(e.error);
            return e.result;
          }, "public command " + String(data.method));
        };
      },
      async cleanup() {
        if (closed) return;
        closed = true;
        try {
          await addon?.uninstall();
        } finally {
          for (
            const tab of [...win.gBrowser.tabs]
          ) {
            if (!original.has(tab)) {
              win.gBrowser.removeTab(tab, { skipPermitUnload: true });
            }
          }
          await IOUtils.remove(path, { ignoreAbsent: true });
        }
        if (
          ExtensionParent.GlobalManager.extensionMap.has(id) ||
          await IOUtils.exists(path)
        ) throw new Error("Owned extension cleanup incomplete");
      },
    };
  } catch (error) {
    await addon?.uninstall();
    await IOUtils.remove(path, { ignoreAbsent: true });
    throw error;
  }
}
