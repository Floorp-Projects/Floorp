// SPDX-License-Identifier: MPL-2.0

import type { Manifest } from "../type.ts";
import { buildSsbKey } from "#libs/pwa/ssbKeyUtils.ts";

export type MacAppOptions = {
  applicationsDir: string;
  profileDir: string;
  executable: string;
};

type MacAppStore = {
  getCurrentSsbData(): Promise<Record<string, Manifest>>;
  saveSsbData(ssb: Manifest): Promise<void>;
  removeSsbData(key: string): Promise<void>;
};

function escapeXml(value: string): string {
  // XML 1.0 Char production, evaluated as code points to preserve non-BMP text.
  // https://www.w3.org/TR/xml/#charsets
  const characters = Array.from(value).filter((character) => {
    const code = character.codePointAt(0)!;
    return code === 0x9 || code === 0xa || code === 0xd ||
      (code >= 0x20 && code <= 0xd7ff) ||
      (code >= 0xe000 && code <= 0xfffd) ||
      (code >= 0x10000 && code <= 0x10ffff);
  });
  return characters.join("")
    .replaceAll("&", "&amp;").replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;").replaceAll('"', "&quot;")
    .replaceAll("'", "&apos;");
}

function quoteShell(value: string): string {
  if (value.includes("\0")) {
    throw new Error("[MacOSSupport] NUL in launcher argument");
  }
  return `'${value.replaceAll("'", `'"'"'`)}'`;
}

export async function getMacAppBundle(ssb: Manifest, options: MacAppOptions) {
  // Profile is part of the identity: copied profiles can contain the same SSB ID.
  const digest = await crypto.subtle.digest(
    "SHA-256",
    new TextEncoder().encode(JSON.stringify([options.profileDir, ssb.id]))
      .buffer as ArrayBuffer,
  );
  const token = Array.from(
    new Uint8Array(digest),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("").slice(0, 24);
  // deno-lint-ignore no-control-regex -- A bundle name must be a single filename.
  const name = ssb.name.replace(/[\x00-\x1f\x7f/:]/g, " ").trim()
    .replace(/^\.+/, "").slice(0, 60) || "Web App";
  const path = PathUtils.join(
    options.applicationsDir,
    `${name} (${token}).app`,
  );
  const entries = {
    CFBundleIdentifier: `one.ablaze.floorp.pwa.${token}`,
    CFBundleName: ssb.name,
    CFBundleDisplayName: ssb.name,
    CFBundleExecutable: "launcher",
    CFBundleIconFile: "app.icns",
    CFBundlePackageType: "APPL",
    CFBundleVersion: "1",
    CFBundleShortVersionString: "1.0",
    CFBundleInfoDictionaryVersion: "6.0",
  };
  const plist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
${
    Object.entries(entries).map(([key, value]) =>
      `<key>${key}</key><string>${escapeXml(value)}</string>`
    ).join("\n")
  }
</dict></plist>\n`;
  // Invoke the installed browser directly so --start-ssb reaches both a running
  // instance and a cold start. Forwarding through `open -a` drops args when open.
  const launcher = `#!/bin/sh\nexec ${
    [
      options.executable,
      "--profile",
      options.profileDir,
      "--start-ssb",
      ssb.id,
    ].map(quoteShell).join(" ")
  }\n`;
  return { path, plist, launcher };
}

// A 512px PNG representation in an ICNS container (ic09).
export function pngToIcns(png: Uint8Array): Uint8Array {
  const result = new Uint8Array(png.length + 16);
  const view = new DataView(result.buffer);
  result.set(new TextEncoder().encode("icns"), 0);
  view.setUint32(4, result.length);
  result.set(new TextEncoder().encode("ic09"), 8);
  view.setUint32(12, png.length + 8);
  result.set(png, 16);
  return result;
}

export class MacOSSupport {
  private static pending = new Map<string, Promise<void>>();

  constructor(private options?: MacAppOptions) {}

  private getOptions(): MacAppOptions {
    if (this.options) return this.options;
    const home = Services.dirsvc.get("Home", Ci.nsIFile).path;
    return {
      applicationsDir: PathUtils.join(home, "Applications", "Floorp Apps"),
      profileDir: PathUtils.profileDir,
      executable: Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
    };
  }

  private static async enqueue(
    path: string,
    operation: () => Promise<void>,
  ): Promise<void> {
    // Keep both filesystem and store changes ordered across browser windows.
    // A failed operation must not prevent a later repair or explicit reinstall.
    const previous = MacOSSupport.pending.get(path) ?? Promise.resolve();
    const pending = previous.catch(() => {}).then(operation);
    MacOSSupport.pending.set(path, pending);
    try {
      await pending;
    } finally {
      if (MacOSSupport.pending.get(path) === pending) {
        MacOSSupport.pending.delete(path);
      }
    }
  }

  async install(ssb: Manifest, store?: MacAppStore): Promise<void> {
    const options = this.getOptions();
    const bundle = await getMacAppBundle(ssb, options);
    await MacOSSupport.enqueue(bundle.path, async () => {
      await this.writeBundle(ssb, options, bundle);
      await store?.saveSsbData(ssb);
    });
  }

  async repair(ssb: Manifest, store: MacAppStore): Promise<void> {
    const options = this.getOptions();
    const bundle = await getMacAppBundle(ssb, options);
    await MacOSSupport.enqueue(bundle.path, async () => {
      const apps = await store.getCurrentSsbData();
      const current = Object.values(apps).find((app) => app.id === ssb.id);
      if (!current) return;
      const currentBundle = await getMacAppBundle(current, options);
      // A window opened before a rename must not recreate the old launcher.
      if (currentBundle.path !== bundle.path) return;
      await this.writeBundle(current, options, currentBundle);
    });
  }

  private async writeBundle(
    ssb: Manifest,
    options: MacAppOptions,
    bundle: Awaited<ReturnType<typeof getMacAppBundle>>,
  ): Promise<void> {
    const contents = PathUtils.join(bundle.path, "Contents");
    const markerPath = PathUtils.join(contents, "floorp.json");
    const marker = JSON.stringify({ version: 1, ssb, ...options });
    const executable = PathUtils.join(contents, "MacOS", "launcher");
    const icon = PathUtils.join(contents, "Resources", "app.icns");
    const plist = PathUtils.join(contents, "Info.plist");
    if (await IOUtils.exists(markerPath)) {
      if (
        await IOUtils.readUTF8(markerPath) === marker &&
        await IOUtils.exists(executable) && await IOUtils.exists(icon) &&
        await IOUtils.exists(plist)
      ) return;
    }

    const iconBytes = await this.createIcon(ssb);
    await IOUtils.makeDirectory(PathUtils.join(contents, "MacOS"), {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.makeDirectory(PathUtils.join(contents, "Resources"), {
      ignoreExisting: true,
    });
    await IOUtils.write(icon, iconBytes);
    await IOUtils.writeUTF8(executable, bundle.launcher);
    await IOUtils.setPermissions(executable, 0o755);
    await IOUtils.writeUTF8(plist, bundle.plist);
    // Write last: interrupted installs are repaired on the next launch.
    await IOUtils.writeUTF8(markerPath, marker);
  }

  protected async createIcon(ssb: Manifest): Promise<Uint8Array> {
    // ShellService.shortcutIconType only supports Windows/Linux. Render PNG
    // explicitly, including SVG and missing/broken site icons, for macOS.
    const fallback = "data:image/svg+xml," + encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512"><rect width="512" height="512" rx="96" fill="#4263eb"/><path d="M128 128h256v64H192v48h160v64H192v80h-64z" fill="white"/></svg>',
    );
    const win = (Services.wm.getMostRecentWindow("navigator:browser") ??
      Services.appShell.hiddenDOMWindow) as Window;
    const image = win.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "img",
    ) as HTMLImageElement;
    // Stored PWA icons are data URIs. Do not fetch arbitrary URLs while repairing
    // launchers at startup.
    image.src = ssb.icon.startsWith("data:") ? ssb.icon : fallback;
    try {
      await image.decode();
    } catch (error) {
      console.warn("[MacOSSupport] Using fallback app icon:", error);
      image.src = fallback;
      await image.decode();
    }
    const canvas = win.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas",
    ) as HTMLCanvasElement;
    canvas.width = canvas.height = 512;
    const context = canvas.getContext("2d");
    if (!context) throw new Error("[MacOSSupport] Cannot render app icon");
    context.drawImage(image, 0, 0, 512, 512);
    if (ssb.userContextId && ssb.userContextId > 0) {
      const { getContainerColorName, resolveContainerColor } = ChromeUtils
        .importESModule(
          "resource://noraneko/modules/pwa/containerDisplay.sys.mjs",
        );
      const color = getContainerColorName(ssb.userContextId);
      if (color) {
        context.beginPath();
        context.arc(416, 416, 80, 0, Math.PI * 2);
        context.fillStyle = resolveContainerColor(color);
        context.fill();
        context.strokeStyle = "white";
        context.lineWidth = 16;
        context.stroke();
      }
    }
    const png = await new Promise<Blob>((resolve, reject) => {
      canvas.toBlob((result) => {
        if (result) resolve(result);
        else reject(new Error("[MacOSSupport] Cannot encode app icon"));
      }, "image/png");
    });
    return pngToIcns(new Uint8Array(await png.arrayBuffer()));
  }

  async uninstall(ssb: Manifest, store?: MacAppStore): Promise<void> {
    const options = this.getOptions();
    const { path } = await getMacAppBundle(ssb, options);
    await MacOSSupport.enqueue(path, async () => {
      await this.removeBundle(ssb, options, path);
      await store?.removeSsbData(
        buildSsbKey(ssb.start_url, ssb.userContextId ?? 0),
      );
    });
  }

  private async removeBundle(
    ssb: Manifest,
    options: MacAppOptions,
    path: string,
  ): Promise<void> {
    const markerPath = PathUtils.join(path, "Contents", "floorp.json");
    if (!await IOUtils.exists(markerPath)) return;
    const marker = await IOUtils.readJSON(markerPath) as {
      profileDir?: string;
      ssb?: { id?: string };
    };
    if (marker.profileDir !== options.profileDir || marker.ssb?.id !== ssb.id) {
      throw new Error(
        "[MacOSSupport] Refusing to remove an unowned app bundle",
      );
    }
    await IOUtils.remove(path, { recursive: true, ignoreAbsent: true });
  }
}
