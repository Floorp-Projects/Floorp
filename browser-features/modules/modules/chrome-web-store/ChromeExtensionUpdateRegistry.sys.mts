/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Protected profile state written only by completed authenticated conversions.
 * Installed manifest markers and reports never create update authorization.
 */
export const CHROME_UPDATE_STATUS_TOPIC =
  "floorp-chrome-extension-update-status-changed";

export function notifyChromeUpdateStatus(addonId: string): void {
  const subject = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  subject.data = addonId;
  Services.obs.notifyObservers(subject, CHROME_UPDATE_STATUS_TOPIC, addonId);
}

export interface UpdateRegistryAddon {
  id: string;
  version: string;
  type: string;
  temporarilyInstalled?: boolean;
  isSystem?: boolean;
  isBuiltin?: boolean;
  getResourceURI(path: string): { spec: string };
}

export interface VerifiedUpdateProof {
  addonId: string;
  version: string;
  xpiSha256: string;
  publicKeySha256: string;
}

export interface ChromeUpdateRegistration extends VerifiedUpdateProof {
  enabled: boolean;
  authorizedAt: number;
  lastCheck?: number;
  nextCheck?: number;
}

export interface ChromeUpdateEligibility {
  addonId: string;
  eligibility: "verified" | "migration-required" | "unavailable";
  enabled: boolean;
  registration?: ChromeUpdateRegistration;
  pendingVersion?: string;
}

interface RegistryFile {
  schemaVersion: 1;
  registrations: ChromeUpdateRegistration[];
  pending: ChromeUpdateRegistration[];
}

const HEX_SHA256 = /^[a-f0-9]{64}$/;
const ADDON_ID = /^([a-p]{32})@chromium-extension$/;

function keyChromeId(hash: string): string {
  return Array.from(hash.slice(0, 32), (digit) =>
    String.fromCharCode(97 + Number.parseInt(digit, 16)),
  ).join("");
}

function validProof(value: unknown): value is VerifiedUpdateProof {
  if (!value || typeof value !== "object") return false;
  const proof = value as Record<string, unknown>;
  const match =
    typeof proof.addonId === "string" ? ADDON_ID.exec(proof.addonId) : null;
  return (
    !!match &&
    typeof proof.version === "string" &&
    proof.version.length > 0 &&
    proof.version.length <= 128 &&
    typeof proof.xpiSha256 === "string" &&
    HEX_SHA256.test(proof.xpiSha256) &&
    typeof proof.publicKeySha256 === "string" &&
    HEX_SHA256.test(proof.publicKeySha256) &&
    keyChromeId(proof.publicKeySha256) === match[1]
  );
}

function validRegistration(value: unknown): value is ChromeUpdateRegistration {
  if (!validProof(value)) return false;
  const record = value as ChromeUpdateRegistration;
  return (
    typeof record.enabled === "boolean" &&
    Number.isFinite(record.authorizedAt) &&
    (record.lastCheck === undefined || Number.isFinite(record.lastCheck)) &&
    (record.nextCheck === undefined || Number.isFinite(record.nextCheck))
  );
}

export async function createVerifiedUpdateProof(
  addonId: string,
  version: string,
  xpiData: ArrayBuffer,
  publicKeySha256: string,
): Promise<VerifiedUpdateProof> {
  const digest = new Uint8Array(await crypto.subtle.digest("SHA-256", xpiData));
  const proof = {
    addonId,
    version,
    publicKeySha256,
    xpiSha256: Array.from(digest, (byte) =>
      byte.toString(16).padStart(2, "0"),
    ).join(""),
  };
  if (!validProof(proof))
    throw new Error("Invalid verified Chrome update proof");
  return proof;
}

function installedHash(addon: UpdateRegistryAddon): string | null {
  if (addon.temporarilyInstalled || addon.isSystem || addon.isBuiltin)
    return null;
  const root = Services.io.newURI(addon.getResourceURI("").spec);
  if (!root.schemeIs("jar")) return null;
  const archive = root.QueryInterface!(Ci.nsIJARURI).JARFile;
  if (!archive.schemeIs("file")) return null;
  const file = archive.QueryInterface!(Ci.nsIFileURL).file;
  if (!file.isFile() || file.fileSize > 600 * 1024 * 1024) return null;
  const stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream,
  );
  try {
    stream.init(file, 0x01, 0, 0);
    const hash = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash,
    );
    hash.initWithString("sha256");
    hash.updateFromStream(stream, 0xffffffff);
    return Array.from(hash.finish(false), (byte) =>
      byte.charCodeAt(0).toString(16).padStart(2, "0"),
    ).join("");
  } finally {
    stream.close();
  }
}

function nativeAddon(id: string): Promise<UpdateRegistryAddon | null> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  );
  return AddonManager.getAddonByID(id) as Promise<UpdateRegistryAddon | null>;
}

export class ChromeExtensionUpdateRegistryClass {
  private readonly registrations = new Map<string, ChromeUpdateRegistration>();
  private readonly pending = new Map<string, ChromeUpdateRegistration>();
  private loaded?: Promise<void>;
  private writes: Promise<void> = Promise.resolve();

  constructor(
    private readonly path = PathUtils.join(
      PathUtils.profileDir,
      "chrome-extension-runtime",
      "update-registrations.json",
    ),
    private readonly getAddon = nativeAddon,
  ) {}

  private load(): Promise<void> {
    return (this.loaded ??= (async () => {
      if (!(await IOUtils.exists(this.path))) return;
      const parsed: unknown = await IOUtils.readJSON(this.path);
      if (!parsed || typeof parsed !== "object")
        throw new Error("Invalid Chrome update registry");
      const value = parsed as Partial<RegistryFile>;
      if (
        value.schemaVersion !== 1 ||
        !Array.isArray(value.registrations) ||
        !Array.isArray(value.pending) ||
        value.registrations.length > 10000 ||
        value.pending.length > 10000
      ) {
        throw new Error("Invalid Chrome update registry");
      }
      for (const record of value.registrations) {
        if (validRegistration(record))
          this.registrations.set(record.addonId, { ...record });
      }
      for (const record of value.pending) {
        if (validRegistration(record))
          this.pending.set(record.addonId, { ...record });
      }
    })());
  }

  private persist(): Promise<void> {
    const value: RegistryFile = {
      schemaVersion: 1,
      registrations: [...this.registrations.values()],
      pending: [...this.pending.values()],
    };
    const json = JSON.stringify(value);
    const write = async () => {
      const directory = PathUtils.parent(this.path);
      if (!directory)
        throw new Error("Invalid Chrome update registry location");
      await IOUtils.makeDirectory(directory, {
        ignoreExisting: true,
        permissions: 0o700,
      });
      await IOUtils.writeUTF8(this.path, json, { tmpPath: `${this.path}.tmp` });
      await IOUtils.setPermissions(this.path, 0o600);
    };
    this.writes = this.writes.catch(() => {}).then(write);
    return this.writes;
  }

  private notify(addonId: string): void {
    notifyChromeUpdateStatus(addonId);
  }

  async listIds(): Promise<string[]> {
    await this.load();
    return [...new Set([...this.registrations.keys(), ...this.pending.keys()])];
  }

  async getEligibility(addonId: string): Promise<ChromeUpdateEligibility> {
    await this.load();
    const match = ADDON_ID.exec(addonId);
    const addon = match && (await this.getAddon(addonId));
    if (
      !addon ||
      addon.type !== "extension" ||
      addon.temporarilyInstalled ||
      addon.isSystem ||
      addon.isBuiltin
    ) {
      return { addonId, eligibility: "unavailable", enabled: false };
    }
    let hash: string | null;
    try {
      hash = installedHash(addon);
    } catch {
      hash = null;
    }
    const pending = this.pending.get(addonId);
    if (
      hash &&
      pending?.xpiSha256 === hash &&
      pending.version === addon.version
    ) {
      this.registrations.set(addonId, { ...pending, authorizedAt: Date.now() });
      this.pending.delete(addonId);
      await this.persist();
    }
    const registration = this.registrations.get(addonId);
    if (
      registration &&
      hash === registration.xpiSha256 &&
      addon.version === registration.version
    ) {
      return {
        addonId,
        eligibility: "verified",
        enabled: registration.enabled,
        registration: { ...registration },
        ...(this.pending.has(addonId)
          ? { pendingVersion: this.pending.get(addonId)!.version }
          : {}),
      };
    }
    return {
      addonId,
      eligibility: "migration-required",
      enabled: false,
      ...(pending ? { pendingVersion: pending.version } : {}),
    };
  }

  /** Call only after the native installer actually completed a verified conversion. */
  async recordInstalled(proof: VerifiedUpdateProof): Promise<void> {
    await this.load();
    if (!validProof(proof))
      throw new Error("Invalid verified Chrome update proof");
    const addon = await this.getAddon(proof.addonId);
    if (
      !addon ||
      addon.version !== proof.version ||
      installedHash(addon) !== proof.xpiSha256
    ) {
      throw new Error(
        "Installed Chrome package differs from the verified conversion",
      );
    }
    const previous =
      this.pending.get(proof.addonId) ?? this.registrations.get(proof.addonId);
    this.registrations.set(proof.addonId, {
      ...proof,
      enabled: previous?.enabled ?? true,
      authorizedAt: Date.now(),
      ...(previous?.lastCheck !== undefined
        ? { lastCheck: previous.lastCheck }
        : {}),
      ...(previous?.nextCheck !== undefined
        ? { nextCheck: previous.nextCheck }
        : {}),
    });
    this.pending.delete(proof.addonId);
    await this.persist();
    this.notify(proof.addonId);
  }

  /** Native staging happens after permission approval; allow exact-hash recovery at restart. */
  async recordPostponed(proof: VerifiedUpdateProof): Promise<void> {
    await this.load();
    if (!validProof(proof))
      throw new Error("Invalid verified Chrome update proof");
    const previous = this.registrations.get(proof.addonId);
    this.pending.set(proof.addonId, {
      ...proof,
      enabled: previous?.enabled ?? true,
      authorizedAt: Date.now(),
      ...(previous?.lastCheck !== undefined
        ? { lastCheck: previous.lastCheck }
        : {}),
      ...(previous?.nextCheck !== undefined
        ? { nextCheck: previous.nextCheck }
        : {}),
    });
    await this.persist();
    this.notify(proof.addonId);
  }

  async clearPending(addonId: string): Promise<void> {
    await this.load();
    if (this.pending.delete(addonId)) {
      await this.persist();
      this.notify(addonId);
    }
  }

  async setEnabled(addonId: string, enabled: boolean): Promise<void> {
    const status = await this.getEligibility(addonId);
    if (status.eligibility !== "verified" || !status.registration) {
      throw new Error(
        "Explicit verified migration is required before enabling automatic updates",
      );
    }
    this.registrations.set(addonId, {
      ...status.registration,
      enabled,
      ...(enabled ? { nextCheck: 0 } : {}),
    });
    const pending = this.pending.get(addonId);
    if (pending) this.pending.set(addonId, { ...pending, enabled });
    await this.persist();
    this.notify(addonId);
  }

  async setSchedule(
    addonId: string,
    lastCheck: number,
    nextCheck: number,
  ): Promise<void> {
    await this.load();
    const registration = this.registrations.get(addonId);
    if (!registration) return;
    this.registrations.set(addonId, { ...registration, lastCheck, nextCheck });
    const pending = this.pending.get(addonId);
    if (pending)
      this.pending.set(addonId, { ...pending, lastCheck, nextCheck });
    await this.persist();
  }

  async remove(addonId: string): Promise<void> {
    await this.load();
    if (!this.registrations.has(addonId) && !this.pending.has(addonId)) return;
    this.registrations.delete(addonId);
    this.pending.delete(addonId);
    await this.persist();
    this.notify(addonId);
  }
}

export const ChromeExtensionUpdateRegistry =
  new ChromeExtensionUpdateRegistryClass();
