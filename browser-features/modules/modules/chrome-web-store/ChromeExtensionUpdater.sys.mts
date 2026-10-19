/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CRXConverterClass } from "./CRXConverter.sys.mts";
import {
  type ChromeDownloadTarget,
  CRX_ACCEPT_HEADER,
  CRX_DOWNLOAD_URLS,
  DOWNLOAD_TIMEOUT_MS,
  INSTALL_TIMEOUT_MS,
  isChromeExtensionId,
  MAX_CRX_SIZE,
  resolveChromeDownloadTarget,
} from "./Constants.sys.mts";
import { validateManifest } from "./ManifestTransformer.sys.mts";
import type { ChromeConversionReport, ChromeManifest } from "./types.ts";
import {
  ChromeExtensionUpdateRegistry,
  ChromeExtensionUpdateRegistryClass,
  createVerifiedUpdateProof,
} from "./ChromeExtensionUpdateRegistry.sys.mts";

interface SourceAddon {
  id: string;
  name: string;
  version: string;
  type: string;
  getResourceURI(path: string): { spec: string };
}

interface UpdateInstall {
  addon?: { id: string; version: string };
  existingAddon?: { id: string; version: string };
  error: number;
  state: number;
  addListener(listener: object): void;
  removeListener(listener: object): void;
  cancel(): void;
}

interface UpdateAddonManager {
  getAddonByID(id: string): Promise<SourceAddon | null>;
  getInstallForFile(file: nsIFile): Promise<UpdateInstall>;
  getAllInstalls(): Promise<UpdateInstall[]>;
  addInstallListener(listener: object): void;
  removeInstallListener(listener: object): void;
  addAddonListener(listener: object): void;
  removeAddonListener(listener: object): void;
  STATE_CANCELLED: number;
  STATE_INSTALLED: number;
  STATE_INSTALL_FAILED: number;
  STATE_DOWNLOAD_FAILED: number;
  beforeShutdown: {
    addBlocker(name: string, blocker: () => Promise<void>): void;
    removeBlocker(blocker: () => Promise<void>): void;
  };
  installAddonFromAOMWithOptions(
    browser: Element,
    uri: null,
    install: UpdateInstall,
    options: { preferUpdateOverInstall: boolean },
  ): void;
}

export interface ChromeUpdateCheck {
  addonId: string;
  currentVersion: string;
  availableVersion?: string;
  status: "available" | "up-to-date";
  warnings: string[];
  kind?: "update" | "reconvert";
}

export interface ChromeUpdateResult {
  status: "installed" | "postponed";
  version: string;
}

interface PreparedUpdate {
  currentVersion: string;
  version: string;
  xpiData: ArrayBuffer;
  report: ChromeConversionReport;
  expires: number;
}

function addonManager(): UpdateAddonManager {
  return (
    ChromeUtils.importESModule(
      "resource://gre/modules/AddonManager.sys.mjs",
    ) as unknown as { AddonManager: UpdateAddonManager }
  ).AddonManager;
}

export function getChromeDownloadTarget(): ChromeDownloadTarget {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  // sysinfo.arch is NSPR's compile-time architecture, so it is not a reliable
  // native-machine architecture under emulation. Do not invent os_arch/NaCl.
  return resolveChromeDownloadTarget(
    AppConstants.platform,
    Services.appinfo.XPCOMABI,
  );
}

export async function downloadChromeExtension(
  id: string,
): Promise<ArrayBuffer> {
  if (!isChromeExtensionId(id)) {
    throw new Error("Invalid Chrome extension ID");
  }
  const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  );
  const target = getChromeDownloadTarget();
  let lastError: unknown;
  for (const url of [
    CRX_DOWNLOAD_URLS.primary(id, target),
    CRX_DOWNLOAD_URLS.fallback(id, target),
  ]) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), DOWNLOAD_TIMEOUT_MS);
    try {
      const response = await fetch(url, {
        credentials: "omit",
        headers: { Accept: CRX_ACCEPT_HEADER },
        signal: controller.signal,
      });
      if (!response.ok || !response.body) {
        await response.body?.cancel();
        throw new Error(
          `Chrome extension download failed (${response.status})`,
        );
      }
      if (Number(response.headers.get("Content-Length")) > MAX_CRX_SIZE) {
        await response.body.cancel();
        throw new Error(
          "Chrome extension download exceeds the package size limit",
        );
      }
      const reader = response.body.getReader();
      const chunks: Uint8Array[] = [];
      let size = 0;
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          size += value.byteLength;
          if (size > MAX_CRX_SIZE) {
            throw new Error(
              "Chrome extension download exceeds the package size limit",
            );
          }
          chunks.push(value);
        }
      } finally {
        await reader.cancel();
        reader.releaseLock();
      }
      const bytes = new Uint8Array(size);
      let offset = 0;
      for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.byteLength;
      }
      return bytes.buffer;
    } catch (error) {
      lastError = error;
    } finally {
      clearTimeout(timeout);
    }
  }
  throw lastError instanceof Error
    ? lastError
    : new Error("Chrome extension download failed");
}

export class ChromeExtensionUpdaterClass {
  private readonly prepared = new Map<string, PreparedUpdate>();
  private readonly checks = new Map<string, symbol>();

  constructor(
    private readonly download = downloadChromeExtension,
    private readonly registry: ChromeExtensionUpdateRegistryClass = ChromeExtensionUpdateRegistry,
  ) {}

  private async source(
    addonId: string,
  ): Promise<{ addon: SourceAddon; chromeId: string }> {
    const match = /^([a-p]{32})@chromium-extension$/.exec(addonId);
    const addon = await addonManager().getAddonByID(addonId);
    if (!match || !addon || addon.type !== "extension") {
      throw new Error(
        "Only installed converted Chrome extensions can be updated",
      );
    }
    const { NetUtil } = ChromeUtils.importESModule(
      "resource://gre/modules/NetUtil.sys.mjs",
    );
    const manifest = await new Promise<ChromeManifest>((resolve, reject) => {
      NetUtil.asyncFetch(
        {
          uri: Services.io.newURI(addon.getResourceURI("manifest.json").spec),
          loadUsingSystemPrincipal: true,
        },
        (input: nsIInputStream, status: number) => {
          try {
            if (
              !Components.isSuccessCode(status) ||
              input.available() > 1024 * 1024
            ) {
              throw new Error(
                "The installed extension manifest could not be read",
              );
            }
            const value: unknown = JSON.parse(
              NetUtil.readInputStreamToString(input, input.available(), {
                charset: "UTF-8",
              }),
            );
            if (!validateManifest(value)) {
              throw new Error("The installed extension manifest is invalid");
            }
            resolve(value);
          } catch (error) {
            reject(error);
          } finally {
            input.close();
          }
        },
      );
    });
    const gecko = manifest.browser_specific_settings?.gecko;
    if (gecko?.id !== addonId) {
      throw new Error(
        "Installed extension identity does not match its manifest",
      );
    }
    const marker = gecko.floorp_chrome_extension;
    if (
      marker &&
      (marker.schema_version !== 1 || marker.original_id !== match[1])
    ) {
      throw new Error("Installed Chrome compatibility metadata is invalid");
    }
    return { addon, chromeId: match[1] };
  }

  async checkForUpdate(
    addonId: string,
    options: { reconvert?: boolean } = {},
  ): Promise<ChromeUpdateCheck> {
    const check = Symbol();
    this.checks.set(addonId, check);
    this.prepared.delete(addonId);
    for (const [id, update] of this.prepared) {
      if (update.expires < Date.now()) this.prepared.delete(id);
    }
    const { addon, chromeId } = await this.source(addonId);
    const currentVersion = addon.version;
    const data = await this.download(chromeId);
    const converted = await new CRXConverterClass().convert(data, chromeId, {
      id: chromeId,
      name: addon.name,
    });
    if (
      !converted.success ||
      !converted.xpiData ||
      !converted.report?.signature
    ) {
      throw (
        converted.error ??
        new Error("Verified Chrome extension conversion failed")
      );
    }
    const current = await this.source(addonId);
    if (this.checks.get(addonId) !== check) {
      throw new Error("A newer update check superseded this request");
    }
    if (current.addon.version !== currentVersion) {
      throw new Error("The extension changed while checking for updates");
    }
    const version = converted.report.originalVersion;
    const comparison = Services.vc.compare(version, currentVersion);
    if (comparison < 0 || (comparison === 0 && !options.reconvert)) {
      return {
        addonId,
        currentVersion,
        status: "up-to-date",
        warnings: converted.warnings ?? [],
      };
    }
    if (this.prepared.size >= 3) {
      this.prepared.delete(this.prepared.keys().next().value!);
    }
    this.prepared.set(addonId, {
      currentVersion,
      version,
      xpiData: converted.xpiData,
      report: converted.report,
      expires: Date.now() + 10 * 60 * 1000,
    });
    return {
      addonId,
      currentVersion,
      availableVersion: version,
      status: "available",
      warnings: converted.warnings ?? [],
      kind: comparison > 0 ? "update" : "reconvert",
    };
  }

  async installUpdate(
    addonId: string,
    browser: Element,
  ): Promise<ChromeUpdateResult> {
    const owner = browser.ownerDocument.defaultView;
    if (!owner || !browser.isConnected || owner.closed) {
      throw new Error("An open browser is required to confirm the update");
    }
    const update = this.prepared.get(addonId);
    this.prepared.delete(addonId);
    if (!update || update.expires < Date.now()) {
      throw new Error("Check for an update before installing it");
    }
    const { addon } = await this.source(addonId);
    if (
      addon.version !== update.currentVersion ||
      update.report.addonId !== addonId
    ) {
      throw new Error("The extension changed; check for an update again");
    }
    const file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append(`floorp-chrome-update-${Services.uuid.generateUUID()}.xpi`);
    file.createUnique(0, 0o600);
    let install: UpdateInstall | undefined;
    let retainedFile = false;
    try {
      const proof = await createVerifiedUpdateProof(
        addonId,
        update.version,
        update.xpiData,
        update.report.signature!.publicKeySha256,
      );
      await IOUtils.write(file.path, new Uint8Array(update.xpiData));
      const manager = addonManager();
      install = await manager.getInstallForFile(file);
      if (
        install.error ||
        install.addon?.id !== addonId ||
        install.addon.version !== update.version ||
        install.existingAddon?.id !== addonId ||
        install.existingAddon.version !== update.currentVersion
      ) {
        install.cancel();
        throw new Error(
          `Chrome update rejected by the Add-on Manager (${install.error})`,
        );
      }
      const candidate = install;
      const registry = this.registry;
      const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
        "resource://gre/modules/Timer.sys.mjs",
      );
      const removeRetainedFile = async () => {
        candidate.removeListener(deferredCleanup);
        manager.beforeShutdown.removeBlocker(removeRetainedFile);
        await IOUtils.remove(file.path, { ignoreAbsent: true });
        retainedFile = false;
      };
      const recordInstalled = async () => {
        try {
          await this.registry.recordInstalled(proof);
        } catch (error) {
          console.error(
            "[ChromeExtensionUpdater] Failed to enroll verified installed update",
            error,
          );
        }
      };
      const cleanupDeferred = () => {
        void Promise.all([
          removeRetainedFile(),
          this.registry.clearPending(addonId),
        ]).catch((error) => console.error("[ChromeExtensionUpdater]", error));
      };
      const deferredCleanup = {
        onInstallEnded: () => {
          void removeRetainedFile().catch((error) =>
            console.error("[ChromeExtensionUpdater]", error),
          );
          void recordInstalled();
        },
        onInstallFailed: cleanupDeferred,
        onInstallCancelled: cleanupDeferred,
        onDownloadCancelled: cleanupDeferred,
        onDownloadFailed: cleanupDeferred,
      };
      const result = await new Promise<ChromeUpdateResult>(
        (resolve, reject) => {
          let settled = false;
          let timer: nsITimer | undefined;
          let ownStarted = false;
          const finish = (
            error?: unknown,
            status: ChromeUpdateResult["status"] = "installed",
          ) => {
            if (settled) return;
            settled = true;
            if (timer) clearTimeout(timer);
            candidate.removeListener(listener);
            manager.removeInstallListener(otherInstalls);
            manager.removeAddonListener(addonChanges);
            owner.removeEventListener("unload", onClose);
            if (error) reject(error);
            else resolve({ status, version: update.version });
          };
          const onClose = () => {
            finish(new Error("The update window was closed"));
            candidate.cancel();
          };
          const listener = {
            onInstallStarted() {
              ownStarted = true;
            },
            onInstallEnded() {
              finish();
            },
            onInstallFailed() {
              finish(new Error("Chrome extension update failed"));
            },
            onInstallCancelled() {
              finish(new Error("Chrome extension update cancelled"));
            },
            onDownloadCancelled() {
              finish(new Error("Chrome extension update cancelled"));
            },
            onDownloadFailed() {
              finish(new Error("Chrome extension update preparation failed"));
            },
            onInstallPostponed() {
              retainedFile = true;
              candidate.addListener(deferredCleanup);
              manager.beforeShutdown.addBlocker(
                "Chrome extension postponed update payload",
                removeRetainedFile,
              );
              void registry
                .recordPostponed(proof)
                .catch((error) =>
                  console.error(
                    "[ChromeExtensionUpdater] Failed to record staged update",
                    error,
                  ),
                )
                .finally(() => finish(undefined, "postponed"));
            },
          };
          const cancel = (message: string) => {
            finish(new Error(message));
            candidate.cancel();
          };
          const otherInstalls = {
            onInstallStarted(other: UpdateInstall) {
              if (
                other !== candidate &&
                (other.addon?.id === addonId ||
                  other.existingAddon?.id === addonId)
              ) {
                cancel(
                  "Another installation changed this extension; check for an update again",
                );
              }
            },
          };
          const addonChanges = {
            onUninstalling(other: { id: string }) {
              if (other.id === addonId && !ownStarted) {
                cancel(
                  "The extension was uninstalled while waiting for update confirmation",
                );
              }
            },
            onInstalled(other: { id: string }) {
              if (other.id === addonId && !ownStarted) {
                cancel(
                  "The extension changed while waiting for update confirmation",
                );
              }
            },
          };
          candidate.addListener(listener);
          manager.addInstallListener(otherInstalls);
          manager.addAddonListener(addonChanges);
          owner.addEventListener("unload", onClose, { once: true });
          timer = setTimeout(() => {
            finish(new Error("Chrome extension update timed out"));
            candidate.cancel();
          }, INSTALL_TIMEOUT_MS);
          manager
            .getAllInstalls()
            .then((installs) => {
              if (settled) return;
              const inactive = new Set([
                manager.STATE_CANCELLED,
                manager.STATE_INSTALLED,
                manager.STATE_INSTALL_FAILED,
                manager.STATE_DOWNLOAD_FAILED,
              ]);
              if (
                installs.some(
                  (other) =>
                    other !== candidate &&
                    !inactive.has(other.state) &&
                    (other.addon?.id === addonId ||
                      other.existingAddon?.id === addonId),
                )
              ) {
                cancel("Another update for this extension is pending");
                return;
              }
              return this.source(addonId).then((latest) => {
                if (settled) return;
                if (latest.addon.version !== update.currentVersion) {
                  cancel("The extension changed; check for an update again");
                  return;
                }
                manager.installAddonFromAOMWithOptions(
                  browser,
                  null,
                  candidate,
                  {
                    preferUpdateOverInstall: true,
                  },
                );
              });
            })
            .catch((error) => {
              finish(error);
              candidate.cancel();
            });
        },
      );
      if (result.status === "installed") await recordInstalled();
      return result;
    } finally {
      if (!retainedFile) {
        await IOUtils.remove(file.path, { ignoreAbsent: true });
      }
    }
  }
}

export const ChromeExtensionUpdater = new ChromeExtensionUpdaterClass();
