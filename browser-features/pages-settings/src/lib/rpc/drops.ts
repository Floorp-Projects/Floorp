/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The settings page runs as an about: page with the system principal, so it can
// reach the privileged Drops module directly (same as lib/rpc/experiments.ts).
// deno-lint-ignore no-explicit-any
declare const ChromeUtils: any;

export interface DropPermission {
  name: string;
  ja: string;
}

export interface DropAttestation {
  who: string;
  identity: string;
  issuer: string;
  ok: boolean;
  reason?: string;
  logIndex?: number;
  rekorUrl?: string;
}

export interface DropFile {
  path: string;
  text: string;
}

export interface DropSourceInfo {
  repo?: string;
  commit?: string;
  commit_time?: string;
  path?: string;
}

export interface DropManifestEntry {
  id: string;
  name: string;
  version: string;
  file: string;
  sha256: string;
  size?: number;
}

export interface DropManifest {
  uuid: string;
  name: string;
  note?: string;
  contact?: string[];
  source?: DropSourceInfo;
  entries: DropManifestEntry[];
  lib?: boolean;
}

export interface DropEntryInspection {
  id: string;
  name: string;
  version: string;
  file: string;
  sha256: string;
  matches: string[];
  chrome: boolean;
  webFrame: boolean;
  permissions: DropPermission[];
  abi: string;
  sandboxed: boolean;
  functions: string[];
  sources: DropFile[];
  files: DropFile[];
}

export interface DropDep {
  name: string;
  uuid: string;
  version: string;
  lib: boolean;
  wasm: boolean;
  attestations: DropAttestation[];
}

export interface DropInspection {
  uuid: string;
  name: string;
  icon?: string | null;
  shots?: { file: string; dataUri: string }[];
  registry: { name: string };
  attestations: DropAttestation[];
  manifest: DropManifest;
  deps: DropDep[];
  entries: DropEntryInspection[];
}

export interface InstalledDrop {
  name?: string;
  ids: string[];
  files: string[];
  versions: string[];
  at: number;
  note?: string;
  registry?: string;
}

export interface CatalogItem {
  uuid: string;
  name: string;
  note: string;
  contact: string[];
  lib: boolean;
  icon: string | null;
  shots: number;
  version: string | null;
  source: DropSourceInfo | null;
  rekor: number | null;
  registry: string;
}

export interface Catalog {
  items: CatalogItem[];
  failed: { registry: string; reason: string }[];
}

export interface VerifyResult {
  ok: boolean;
  checked: number;
  bad: string[];
}

const getDropsModule = () => {
  if (typeof ChromeUtils === "undefined") {
    // In the localhost dev server the page is not a chrome-privileged about: page,
    // so there is no way to reach the module. A built browser is needed.
    throw new Error("Drops are only available in a built browser");
  }
  return ChromeUtils.importESModule("resource://noraneko/modules/Drops.sys.mjs");
};

export const dropsRpc = {
  listCatalog(): Promise<Catalog> {
    return getDropsModule().listCatalog();
  },
  /** 手元に置いてある棚(通信しない)。開いた瞬間に並べるため */
  listCatalogCached(): Promise<CatalogItem[]> {
    return getDropsModule().listCatalogCached();
  },
  /** hover した一枚の manifest を、押す前に温める */
  async prefetchDrop(uuid: string, registry?: string): Promise<void> {
    await getDropsModule().prefetchDrop(uuid, registry);
  },
  inspectDrop(uuid: string, registry?: string): Promise<DropInspection> {
    return getDropsModule().inspectDrop(uuid, registry);
  },
  verifyDrop(inspected: DropInspection): Promise<VerifyResult> {
    return getDropsModule().verifyDrop(inspected);
  },
  async installDrop(inspected: DropInspection): Promise<string[]> {
    return await getDropsModule().installDrop(inspected);
  },
  async removeDrop(uuid: string): Promise<void> {
    await getDropsModule().removeDrop(uuid);
  },
  listDrops(): Record<string, InstalledDrop> {
    return getDropsModule().listDrops();
  },
  parseUuid(ref: string): string {
    return getDropsModule().parseUuid(ref);
  },
};

/** 一枚の判が全部そろっているか(registry と、使う library のぶんも) */
export function allAttested(d: DropInspection): boolean {
  const ok = (a: DropAttestation[]) => a.length > 0 && a.every((x) => x.ok);
  return ok(d.attestations) && (d.deps ?? []).every((dep) => ok(dep.attestations));
}

/** 1.3.0 と 1.10.0 を数で比べる(字で比べると 10 < 3 になる) */
export function newer(
  a: string | null | undefined,
  b: string | null | undefined,
): boolean {
  if (!a || !b) {
    return false;
  }
  const xs = a.split(".").map(Number);
  const ys = b.split(".").map(Number);
  for (let i = 0; i < Math.max(xs.length, ys.length); i++) {
    const x = xs[i] ?? 0;
    const y = ys[i] ?? 0;
    if (Number.isNaN(x) || Number.isNaN(y)) {
      return false;
    }
    if (x !== y) {
      return x > y;
    }
  }
  return false;
}
