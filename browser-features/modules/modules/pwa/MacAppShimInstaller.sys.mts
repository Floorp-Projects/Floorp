// SPDX-License-Identifier: MPL-2.0

import { AppRegistry } from "./AppRegistry.sys.mts";
import { getMacAppBundle, MacOSSupport } from "./supports/MacOS.sys.mts";
import type { MacAppOptions, MacAppStore } from "./supports/MacOS.sys.mts";
import { selectMacAppIntegration } from "#libs/pwa/appRegistry.ts";
import { classifyBundleRecovery } from "#libs/pwa/bundleRecovery.ts";
import type {
  AppRegistryState,
  InstalledAppShim,
  MacAppShimCapabilities,
} from "#libs/pwa/appRegistryTypes.ts";
import type { AppMutationScope } from "./NativeAppRuntime.sys.mts";
import type { Manifest } from "./type.ts";
import type { NativeAppService } from "#libs/pwa/nativeAppRuntimeTypes.ts";

export interface MacAppShimInstallerPaths {
  profileDirectory: string;
  applicationsDirectory: string;
  browserExecutable: string;
  shimExecutable: string;
}

export interface PreparedMacAppShim {
  profileId: string;
  appId: string;
  bundlePath: string;
  liveBundlePath: string;
  transactionId?: string;
  reusedInstalled: boolean;
}

type InstallJournal = {
  schemaVersion: 1;
  phase: "prepared" | "committed" | "removing";
  appId: string;
  profileId: string;
  bundleId: string;
  transactionId: string;
  livePath: string;
  stagePath: string;
  backupPath: string;
  previousFingerprint: string;
  candidateFingerprint: string;
  cdHash: string;
  name: string;
  previousName: string;
  previous?: InstallJournal;
};

function xml(value: string): string {
  // XML 1.0 Char production, evaluated as code points to preserve non-BMP text,
  // exactly as the legacy macOS bundle generator does. A name containing a
  // noncharacter or an unpaired surrogate would otherwise produce a plist that
  // neither codesign nor the Shim can read.
  // https://www.w3.org/TR/xml/#charsets
  const characters = Array.from(value).filter((character) => {
    const code = character.codePointAt(0)!;
    return code === 0x9 || code === 0xa || code === 0xd ||
      (code >= 0x20 && code <= 0xd7ff) || (code >= 0xe000 && code <= 0xfffd) ||
      (code >= 0x10000 && code <= 0x10ffff);
  });
  return characters.join("")
    .replaceAll("&", "&amp;").replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll(
      "'",
      "&apos;",
    );
}

function object(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("[MacAppShimInstaller] Invalid native metadata");
  }
  return value as Record<string, unknown>;
}

function string(value: unknown): string {
  if (
    typeof value !== "string" || !value ||
    // deno-lint-ignore no-control-regex -- Reject characters forbidden in XML metadata.
    /[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(value)
  ) {
    throw new Error("[MacAppShimInstaller] Invalid metadata string");
  }
  return value;
}

/** Explicit, capability-gated installation. Never called by legacy repair. */
export class MacAppShimInstaller {
  private static operations = new Map<string, Promise<unknown>>();

  constructor(
    private readonly service: NativeAppService,
    private readonly paths?: MacAppShimInstallerPaths,
  ) {}

  static forMaintenance(options: MacAppOptions): MacAppShimInstaller {
    const contract = "@floorp.org/mac-web-app-service;1";
    if (!(contract in Cc)) {
      throw new Error(
        "[MacAppShimInstaller] Native Runtime is required to modify a signed Web App",
      );
    }
    const interfaces = Ci as unknown as Record<string, nsIID>;
    const service = Cc[contract].getService(
      interfaces.nsIMacWebAppService,
    ) as unknown as NativeAppService;
    if (service.protocolVersion !== 1) {
      throw new Error("[MacAppShimInstaller] Unsupported native Runtime");
    }
    return new MacAppShimInstaller(service, {
      profileDirectory: options.profileDir,
      applicationsDirectory: options.applicationsDir,
      browserExecutable: options.executable,
      shimExecutable: PathUtils.join(
        Services.dirsvc.get("GreBinD", Ci.nsIFile).path,
        "floorp-app-shim",
      ),
    });
  }

  private get profileDirectory(): string {
    return this.paths?.profileDirectory ?? PathUtils.profileDir;
  }

  private registry() {
    return AppRegistry.getForProfile(this.profileDirectory);
  }

  private async withMutation<T>(
    appId: string,
    scope: AppMutationScope | undefined,
    operation: () => Promise<T>,
  ): Promise<T | null> {
    const { NativeAppRuntime } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
    );
    if (scope) {
      NativeAppRuntime.assertMutationScope(appId, scope);
      return await operation();
    }
    return await NativeAppRuntime.withMutation(appId, operation);
  }

  private queue<T>(appId: string, operation: () => Promise<T>): Promise<T> {
    const key = `${this.profileDirectory}:${appId}`;
    const previous = MacAppShimInstaller.operations.get(key) ??
      Promise.resolve();
    const pending = previous.catch(() => {}).then(operation);
    MacAppShimInstaller.operations.set(key, pending);
    return pending.finally(() => {
      if (MacAppShimInstaller.operations.get(key) === pending) {
        MacAppShimInstaller.operations.delete(key);
      }
    });
  }

  private supported(): MacAppShimCapabilities | null {
    if (
      Services.appinfo.OS !== "Darwin" ||
      !Services.prefs.getBoolPref(
        "floorp.browser.nativeApp.appShim.enabled",
        false,
      ) ||
      this.service.protocolVersion !== 1
    ) return null;
    const capabilities = JSON.parse(
      this.service.capabilitiesJSON,
    ) as MacAppShimCapabilities;
    return selectMacAppIntegration(capabilities) === "app-shim"
      ? capabilities
      : null;
  }

  private async journalPath(appId: string): Promise<string> {
    const digest = new Uint8Array(
      await crypto.subtle.digest(
        "SHA-256",
        new TextEncoder().encode(appId).buffer as ArrayBuffer,
      ),
    );
    const token = Array.from(
      digest,
      (byte) => byte.toString(16).padStart(2, "0"),
    ).join("");
    return PathUtils.join(
      this.profileDirectory,
      "ssb",
      "app-shim-transactions",
      `${token}.json`,
    );
  }

  private async writeJournal(journal: InstallJournal): Promise<void> {
    const path = await this.journalPath(journal.appId);
    await IOUtils.makeDirectory(PathUtils.parent(path)!, {
      createAncestors: true,
      ignoreExisting: true,
      permissions: 0o700,
    });
    await IOUtils.writeJSON(path, journal, {
      tmpPath: `${path}.tmp`,
      flush: true,
    });
  }

  private async readJournal(
    appId: string,
    registry: AppRegistryState,
  ): Promise<InstallJournal | null> {
    const path = await this.journalPath(appId);
    if (!await IOUtils.exists(path)) return null;
    return this.validateJournal(await IOUtils.readJSON(path), appId, registry);
  }

  private validateJournal(
    value: unknown,
    appId: string,
    registry: AppRegistryState,
    nested = false,
  ): InstallJournal {
    const input = object(value);
    const app = registry.apps.find((entry) => entry.installId === appId);
    const transactionId = string(input.transactionId);
    if (
      !app || input.schemaVersion !== 1 || input.appId !== appId ||
      input.profileId !== registry.profile.id ||
      input.bundleId !== app.bundleId ||
      input.livePath !== app.bundlePath ||
      !/^[A-Za-z0-9-]{1,80}$/.test(transactionId) ||
      (input.phase !== "prepared" && input.phase !== "committed" &&
        input.phase !== "removing")
    ) {
      throw new Error(
        "[MacAppShimInstaller] Journal does not match the registered application",
      );
    }
    const parent = PathUtils.parent(app.bundlePath)!;
    if (
      input.stagePath !==
        PathUtils.join(parent, `.floorp-stage-${transactionId}.app`) ||
      input.backupPath !==
        PathUtils.join(parent, `.floorp-backup-${transactionId}.app`) ||
      !/^[a-f0-9]{64}$/.test(string(input.previousFingerprint)) ||
      !/^[a-f0-9]{64}$/.test(string(input.candidateFingerprint)) ||
      !/^[a-f0-9]{40,64}$/.test(string(input.cdHash))
    ) {
      throw new Error(
        "[MacAppShimInstaller] Invalid transaction paths or fingerprints",
      );
    }
    string(input.name);
    string(input.previousName);
    if (input.previous !== undefined) {
      if (nested) {
        throw new Error(
          "[MacAppShimInstaller] Nested recovery history is invalid",
        );
      }
      const previous = this.validateJournal(
        input.previous,
        appId,
        registry,
        true,
      );
      if (
        previous.phase !== "committed" ||
        previous.candidateFingerprint !== input.previousFingerprint ||
        previous.transactionId === transactionId
      ) {
        throw new Error(
          "[MacAppShimInstaller] Previous receipt does not match the replacement",
        );
      }
    }
    return input as unknown as InstallJournal;
  }

  private receipt(journal: InstallJournal): InstalledAppShim {
    return {
      transactionId: journal.transactionId,
      shimVersion: "1",
      cdHash: journal.cdHash,
      fingerprint: journal.candidateFingerprint,
    };
  }

  private metadataStore(): MacAppStore {
    if (this.profileDirectory !== PathUtils.profileDir) {
      throw new Error(
        "[MacAppShimInstaller] Recovery store must belong to this browser profile",
      );
    }
    const { DataManager } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/DataStore.sys.mjs",
    );
    return new DataManager();
  }

  private async saveName(
    journal: InstallJournal,
    name: string,
    store?: MacAppStore,
  ): Promise<void> {
    const target = store ?? this.metadataStore();
    const current = Object.values(await target.getCurrentSsbData()).find((
      app,
    ) => app.id === journal.appId);
    if (!current) {
      throw new Error(
        "[MacAppShimInstaller] Application was removed during metadata recovery",
      );
    }
    await target.saveSsbData({ ...current, name, short_name: name });
  }

  private verifyInstalled(journal: InstallJournal): void {
    const verified = object(
      JSON.parse(this.service.verifyAppBundle(journal.appId, journal.livePath)),
    );
    if (
      verified.fingerprint !== journal.candidateFingerprint ||
      verified.cdHash !== journal.cdHash ||
      verified.bundleId !== journal.bundleId
    ) {
      throw new Error(
        "[MacAppShimInstaller] Installed Shim changed; refusing automatic replacement",
      );
    }
  }

  private async sign(path: string, bundleId: string): Promise<void> {
    const { Subprocess } = ChromeUtils.importESModule(
      "resource://gre/modules/Subprocess.sys.mjs",
    );
    const process = await Subprocess.call({
      command: "/usr/bin/codesign",
      arguments: [
        "--force",
        "--sign",
        "-",
        "--identifier",
        bundleId,
        "--timestamp=none",
        path,
      ],
      stderr: "stdout",
    });
    let output = "";
    for (;;) {
      const chunk = await process.stdout.readString();
      if (!chunk) break;
      output = (output + chunk).slice(-16384);
    }
    const { exitCode } = await process.wait();
    if (exitCode !== 0) {
      throw new Error(
        `[MacAppShimInstaller] Signing failed (${exitCode}): ${output}`,
      );
    }
    await this.clearGeneratedQuarantine(path);
  }

  private async fingerprint(path: string): Promise<string | null> {
    return await IOUtils.exists(path)
      ? this.service.fingerprintBundle(path)
      : null;
  }

  private async clearGeneratedQuarantine(path: string): Promise<void> {
    const attribute = "com.apple.quarantine";
    if (await IOUtils.hasMacXAttr(path, attribute)) {
      await IOUtils.delMacXAttr(path, attribute);
    }
    if ((await IOUtils.stat(path)).type === "directory") {
      for (const child of await IOUtils.getChildren(path)) {
        await this.clearGeneratedQuarantine(child);
      }
    }
  }

  private async restore(
    journal: InstallJournal,
    store?: MacAppStore,
  ): Promise<void> {
    const state = classifyBundleRecovery(
      await this.fingerprint(journal.livePath),
      await this.fingerprint(journal.stagePath),
      await this.fingerprint(journal.backupPath),
      journal.previousFingerprint,
      journal.candidateFingerprint,
    );
    if (state === "swapped") {
      this.service.exchangeAppBundles(
        journal.livePath,
        journal.backupPath,
        journal.candidateFingerprint,
        journal.previousFingerprint,
      );
      this.service.removeStagedBundle(
        journal.backupPath,
        journal.candidateFingerprint,
      );
    } else if (state === "staged") {
      this.service.removeStagedBundle(
        journal.stagePath,
        journal.candidateFingerprint,
      );
    } else if (state === "ready-to-swap") {
      this.service.removeStagedBundle(
        journal.backupPath,
        journal.candidateFingerprint,
      );
    } else if (state !== "rolled-back") {
      throw new Error(
        "[MacAppShimInstaller] Recovery stopped because bundle contents changed",
      );
    }
    if (journal.previous) {
      await this.saveName(journal, journal.previousName, store);
    }
    await this.registry().restoreInstallation(
      journal.appId,
      journal.transactionId,
      journal.previous ? this.receipt(journal.previous) : null,
      journal.previousName,
    );
    if (journal.previous) await this.writeJournal(journal.previous);
    else {await IOUtils.remove(await this.journalPath(journal.appId), {
        ignoreAbsent: true,
      });}
  }

  prepare(ssb: Manifest): Promise<PreparedMacAppShim | null> {
    return this.queue(ssb.id, async () => {
      const capabilities = this.supported();
      if (!capabilities) return null;
      const registry = this.registry();
      let state = await registry.read();
      const previous = state ? await this.readJournal(ssb.id, state) : null;
      if (state && previous) {
        this.service.configure(state.profile.id);
        if (previous.phase === "removing") {
          throw new Error(
            "[MacAppShimInstaller] Uninstall must finish before this app can launch",
          );
        }
        if (previous.phase === "committed") {
          this.verifyInstalled(previous);
          await registry.completeMigration(
            ssb.id,
            this.receipt(previous),
            previous.name,
          );
          return {
            profileId: state.profile.id,
            appId: ssb.id,
            bundlePath: previous.livePath,
            liveBundlePath: previous.livePath,
            reusedInstalled: true,
          };
        }
        await this.restore(previous);
        state = (await registry.read())!;
        const restored = await this.readJournal(ssb.id, state);
        if (restored) {
          this.verifyInstalled(restored);
          return {
            profileId: state.profile.id,
            appId: ssb.id,
            bundlePath: restored.livePath,
            liveBundlePath: restored.livePath,
            reusedInstalled: true,
          };
        }
      }
      const home = Services.dirsvc.get("Home", Ci.nsIFile).path;
      const options = {
        applicationsDir: this.paths?.applicationsDirectory ??
          PathUtils.join(home, "Applications", "Floorp Apps"),
        profileDir: this.profileDirectory,
        executable: this.paths?.browserExecutable ??
          Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
      };
      const legacy = await getMacAppBundle(ssb, options);
      const support = new MacOSSupport(options);
      if (!await IOUtils.exists(legacy.path)) {
        await support.installForAppShim(ssb);
      }
      state = await support.prepareAppShimRegistration(ssb, capabilities);
      if (!state) {
        throw new Error("[MacAppShimInstaller] Native enrollment unavailable");
      }
      this.service.configure(state.profile.id);
      const app = state.apps.find((entry) => entry.installId === ssb.id)!;
      if (app.pendingMigration) {
        // A journal is written before registry intent. Without one, a pending
        // record cannot authorize replacement or deletion of arbitrary files.
        throw new Error(
          "[MacAppShimInstaller] Unresolved migration intent requires recovery",
        );
      }
      const transactionId = crypto.randomUUID();
      const parent = PathUtils.parent(legacy.path)!;
      const stagePath = PathUtils.join(
        parent,
        `.floorp-stage-${transactionId}.app`,
      );
      const backupPath = PathUtils.join(
        parent,
        `.floorp-backup-${transactionId}.app`,
      );
      const previousFingerprint = this.service.fingerprintBundle(legacy.path);
      const host = object(JSON.parse(this.service.hostIdentityJSON));
      const executable = this.paths?.shimExecutable ?? PathUtils.join(
        Services.dirsvc.get("GreBinD", Ci.nsIFile).path,
        "floorp-app-shim",
      );
      if (!await IOUtils.exists(executable)) {
        throw new Error(
          "[MacAppShimInstaller] Packaged native executable is missing",
        );
      }
      await IOUtils.makeDirectory(stagePath, {
        ignoreExisting: false,
        permissions: 0o700,
      });
      try {
        const contents = PathUtils.join(stagePath, "Contents");
        const macOS = PathUtils.join(contents, "MacOS");
        const resources = PathUtils.join(contents, "Resources");
        await IOUtils.makeDirectory(macOS, { createAncestors: true });
        await IOUtils.makeDirectory(resources);
        await IOUtils.copy(executable, PathUtils.join(macOS, "app-shim"));
        await IOUtils.setPermissions(PathUtils.join(macOS, "app-shim"), 0o755);
        await IOUtils.copy(
          PathUtils.join(legacy.path, "Contents", "Resources", "app.icns"),
          PathUtils.join(resources, "app.icns"),
        );
        const entries = {
          CFBundleIdentifier: legacy.bundleId,
          CFBundleName: ssb.name,
          CFBundleDisplayName: ssb.name,
          CFBundleExecutable: "app-shim",
          CFBundleIconFile: "app.icns",
          CFBundlePackageType: "APPL",
          CFBundleVersion: "1",
          CFBundleShortVersionString: "1.0",
          CFBundleInfoDictionaryVersion: "6.0",
          NSPrincipalClass: "NSApplication",
          FloorpAppShimAppId: ssb.id,
          FloorpAppShimProfileId: state.profile.id,
          FloorpAppShimProfilePath: this.profileDirectory,
          FloorpAppShimHostBundleIdentifier: string(host.identifier),
          FloorpAppShimHostCodeRequirement: string(host.designatedRequirement),
          FloorpAppShimHostPath: string(host.bundlePath),
        };
        const plist = '<?xml version="1.0" encoding="UTF-8"?>\n' +
          '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n' +
          '<plist version="1.0"><dict>\n' +
          Object.entries(entries).map(([key, value]) =>
            `<key>${key}</key><string>${xml(string(value))}</string>`
          ).join("\n") +
          "\n<key>NSHighResolutionCapable</key><true/>\n</dict></plist>\n";
        await IOUtils.writeUTF8(PathUtils.join(contents, "Info.plist"), plist);
        await IOUtils.writeJSON(PathUtils.join(resources, "floorp.json"), {
          version: 4,
          integration: "app-shim",
          transactionId,
          profileId: state.profile.id,
          profileDir: this.profileDirectory,
          ssb,
        });
        await this.sign(stagePath, legacy.bundleId);
        const verified = object(
          JSON.parse(this.service.verifyAppBundle(ssb.id, stagePath)),
        );
        if (verified.bundleId !== legacy.bundleId) {
          throw new Error("[MacAppShimInstaller] Signed bundle ID mismatch");
        }
        const journal: InstallJournal = {
          schemaVersion: 1,
          phase: "prepared",
          appId: ssb.id,
          profileId: state.profile.id,
          bundleId: legacy.bundleId,
          transactionId,
          livePath: legacy.path,
          stagePath,
          backupPath,
          previousFingerprint,
          candidateFingerprint: string(verified.fingerprint),
          cdHash: string(verified.cdHash),
          name: ssb.name,
          previousName: ssb.name,
        };
        await this.writeJournal(journal);
        await registry.beginMigration(ssb.id, transactionId, "1", capabilities);
        return {
          profileId: state.profile.id,
          appId: ssb.id,
          bundlePath: stagePath,
          liveBundlePath: legacy.path,
          transactionId,
          reusedInstalled: false,
        };
      } catch (error) {
        // This path was created exclusively by this attempt; the live app is
        // still untouched. No launcher is exchanged until commit is requested.
        await IOUtils.remove(stagePath, {
          recursive: true,
          ignoreAbsent: true,
        });
        throw error;
      }
    });
  }

  /** Called after authenticated connection and a successfully presented window. */
  commit(appId: string): Promise<void> {
    return this.queue(appId, async () => {
      const state = await this.registry().read();
      if (!state) throw new Error("[MacAppShimInstaller] Missing app registry");
      const journal = await this.readJournal(appId, state);
      if (!journal || journal.phase === "committed") return;
      this.service.moveStagedBundle(
        journal.stagePath,
        journal.backupPath,
        journal.candidateFingerprint,
      );
      this.service.exchangeAppBundles(
        journal.livePath,
        journal.backupPath,
        journal.previousFingerprint,
        journal.candidateFingerprint,
      );
      this.service.adoptAppBundlePath(appId, journal.livePath);
      journal.phase = "committed";
      await this.writeJournal(journal);
      await this.registry().completeMigration(
        appId,
        this.receipt(journal),
        journal.name,
      );
      // Retain the fingerprint-checked old bundle for explicit rollback.
    });
  }

  /** The caller must terminate this app's Shim before rolling its bundle back. */
  rollback(appId: string, expectedTransactionId?: string): Promise<void> {
    return this.queue(appId, async () => {
      const state = await this.registry().read();
      if (!state) return;
      const journal = await this.readJournal(appId, state);
      if (journal) {
        if (
          expectedTransactionId &&
          journal.transactionId !== expectedTransactionId
        ) {
          throw new Error(
            "[MacAppShimInstaller] Refusing rollback of a different transaction",
          );
        }
        this.service.configure(state.profile.id);
        await this.restore(journal);
      }
    });
  }

  /** A rename changes sealed display metadata while retaining the installed identity. */
  async rename(
    ssb: Manifest,
    updated: Manifest,
    store: MacAppStore,
    scope?: AppMutationScope,
  ): Promise<boolean> {
    if (
      ssb.id !== updated.id || ssb.start_url !== updated.start_url ||
      (ssb.userContextId ?? 0) !== (updated.userContextId ?? 0)
    ) {
      throw new Error(
        "[MacAppShimInstaller] Rename cannot change the app's session identity",
      );
    }
    string(updated.name);
    const { NativeAppRuntime } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
    );
    const result = await this.withMutation(
      ssb.id,
      scope,
      () =>
        this.queue(ssb.id, async () => {
          if (scope) NativeAppRuntime.assertMutationScope(ssb.id, scope);
          const registry = this.registry();
          let state = await registry.read();
          if (!state) {
            throw new Error("[MacAppShimInstaller] Missing native registry");
          }
          this.service.configure(state.profile.id);
          let previous = await this.readJournal(ssb.id, state);
          if (!previous || previous.phase === "removing") {
            throw new Error(
              "[MacAppShimInstaller] App is not available for rename",
            );
          }
          if (previous.phase === "prepared") {
            await this.restore(previous, store);
            state = (await registry.read())!;
            previous = await this.readJournal(ssb.id, state);
          }
          if (!previous || previous.phase !== "committed") {
            throw new Error(
              "[MacAppShimInstaller] A native installation must finish before rename",
            );
          }
          this.verifyInstalled(previous);
          if (previous.name === updated.name) {
            await store.saveSsbData(updated);
            return true;
          }
          // Keep one rollback generation. The previous receipt stays valid even
          // after its older backup has been deliberately retired.
          if (await IOUtils.exists(previous.backupPath)) {
            this.service.removeStagedBundle(
              previous.backupPath,
              previous.previousFingerprint,
            );
          }
          const { previous: _older, ...priorReceipt } = previous;
          const transactionId = crypto.randomUUID();
          const parent = PathUtils.parent(previous.livePath)!;
          const stagePath = PathUtils.join(
            parent,
            `.floorp-stage-${transactionId}.app`,
          );
          const backupPath = PathUtils.join(
            parent,
            `.floorp-backup-${transactionId}.app`,
          );
          if (await IOUtils.exists(stagePath)) {
            throw new Error(
              "[MacAppShimInstaller] Staging path already exists",
            );
          }
          await IOUtils.copy(previous.livePath, stagePath, { recursive: true });
          let journal: InstallJournal | null = null;
          try {
            const contents = PathUtils.join(stagePath, "Contents");
            const plistPath = PathUtils.join(contents, "Info.plist");
            let plist = await IOUtils.readUTF8(plistPath);
            for (const key of ["CFBundleName", "CFBundleDisplayName"]) {
              const field = new RegExp(
                `(<key>${key}</key>\\s*<string>)[\\s\\S]*?</string>`,
              );
              if (!field.test(plist)) {
                throw new Error(
                  "[MacAppShimInstaller] Signed display metadata is missing",
                );
              }
              plist = plist.replace(field, (_match, opening: string) =>
                `${opening}${xml(updated.name)}</string>`);
            }
            await IOUtils.writeUTF8(plistPath, plist);
            const markerPath = PathUtils.join(
              contents,
              "Resources",
              "floorp.json",
            );
            const marker = object(await IOUtils.readJSON(markerPath));
            if (
              marker.profileId !== state.profile.id ||
              object(marker.ssb).id !== ssb.id
            ) {
              throw new Error(
                "[MacAppShimInstaller] Native bundle marker changed",
              );
            }
            await IOUtils.writeJSON(markerPath, {
              ...marker,
              ssb: updated,
              transactionId,
            });
            await this.sign(stagePath, previous.bundleId);
            const verified = object(
              JSON.parse(this.service.verifyAppBundle(ssb.id, stagePath)),
            );
            if (verified.bundleId !== previous.bundleId) {
              throw new Error(
                "[MacAppShimInstaller] Replacement changed bundle identity",
              );
            }
            journal = {
              schemaVersion: 1,
              phase: "prepared",
              appId: ssb.id,
              profileId: state.profile.id,
              bundleId: previous.bundleId,
              transactionId,
              livePath: previous.livePath,
              stagePath,
              backupPath,
              previousFingerprint: previous.candidateFingerprint,
              candidateFingerprint: string(verified.fingerprint),
              cdHash: string(verified.cdHash),
              name: updated.name,
              previousName: previous.name,
              previous: priorReceipt,
            };
            await this.writeJournal(journal);
            await registry.beginMigration(
              ssb.id,
              transactionId,
              "1",
              JSON.parse(
                this.service.capabilitiesJSON,
              ) as MacAppShimCapabilities,
            );
            this.service.moveStagedBundle(
              stagePath,
              backupPath,
              journal.candidateFingerprint,
            );
            this.service.exchangeAppBundles(
              journal.livePath,
              backupPath,
              journal.previousFingerprint,
              journal.candidateFingerprint,
            );
            await store.saveSsbData(updated);
            journal.phase = "committed";
            await this.writeJournal(journal);
            await registry.completeMigration(
              ssb.id,
              this.receipt(journal),
              journal.name,
            );
            return true;
          } catch (error) {
            if (journal) {
              await this.restore(journal, store);
            } else {await IOUtils.remove(stagePath, {
                recursive: true,
                ignoreAbsent: true,
              });}
            throw error;
          }
        }),
    );
    return result === true;
  }

  async uninstall(
    ssb: Manifest,
    store?: MacAppStore,
    scope?: AppMutationScope,
  ): Promise<void> {
    const { NativeAppRuntime } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
    );
    const result = await this.withMutation(
      ssb.id,
      scope,
      () =>
        this.queue(ssb.id, async () => {
          if (scope) NativeAppRuntime.assertMutationScope(ssb.id, scope);
          const registry = this.registry();
          let state = await registry.read();
          if (!state) {
            throw new Error("[MacAppShimInstaller] Missing native registry");
          }
          this.service.configure(state.profile.id);
          let journal = await this.readJournal(ssb.id, state);
          if (!journal) {
            const app = state.apps.find((entry) => entry.installId === ssb.id);
            // Cancelling an in-flight launch can finish its rollback before
            // this queued uninstall runs. Only an explicitly restored legacy
            // registration permits the filesystem fallback below.
            if (app?.integration !== "launcher" || app.pendingMigration) {
              throw new Error(
                "[MacAppShimInstaller] Missing native installation receipt",
              );
            }
          }
          if (journal?.phase === "prepared") {
            await this.restore(journal, store);
            state = (await registry.read())!;
            journal = await this.readJournal(ssb.id, state);
          }
          if (!journal) {
            const home = Services.dirsvc.get("Home", Ci.nsIFile).path;
            await new MacOSSupport({
              profileDir: this.profileDirectory,
              applicationsDir: this.paths?.applicationsDirectory ??
                PathUtils.join(home, "Applications", "Floorp Apps"),
              executable: this.paths?.browserExecutable ??
                Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
            }).uninstallAfterAppShimRollback(ssb, store);
            return true;
          }
          if (journal.phase === "committed") {
            this.verifyInstalled(journal);
            journal.phase = "removing";
            await this.writeJournal(journal);
          }
          const live = await this.fingerprint(journal.livePath);
          const staged = await this.fingerprint(journal.stagePath);
          if (
            live !== null &&
            (live !== journal.candidateFingerprint || staged !== null)
          ) {
            throw new Error(
              "[MacAppShimInstaller] Refusing removal of changed bundle contents",
            );
          }
          if (staged !== null && staged !== journal.candidateFingerprint) {
            throw new Error(
              "[MacAppShimInstaller] Uninstall staging contents changed",
            );
          }
          if (live !== null) {
            this.service.retireAppBundle(
              journal.livePath,
              journal.stagePath,
              journal.candidateFingerprint,
            );
          }
          if (await IOUtils.exists(journal.stagePath)) {
            this.service.removeStagedBundle(
              journal.stagePath,
              journal.candidateFingerprint,
            );
          }
          if (await IOUtils.exists(journal.backupPath)) {
            this.service.removeStagedBundle(
              journal.backupPath,
              journal.previousFingerprint,
            );
          }
          const target = store ?? this.metadataStore();
          const { buildSsbKey } = await import("#libs/pwa/ssbKeyUtils.ts");
          await target.removeSsbData(
            buildSsbKey(ssb.start_url, ssb.userContextId ?? 0),
          );
          await registry.forgetApp(ssb.id, journal.livePath);
          await IOUtils.remove(await this.journalPath(ssb.id), {
            ignoreAbsent: true,
          });
          return true;
        }),
    );
    if (result !== true) {
      throw new Error(
        "[MacAppShimInstaller] App declined to close; uninstall cancelled",
      );
    }
  }
}
