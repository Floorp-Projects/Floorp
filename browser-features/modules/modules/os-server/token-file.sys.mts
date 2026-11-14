// SPDX-License-Identifier: MPL-2.0

import {
  PROFILE_TOKEN_DIRECTORY,
  PROFILE_TOKEN_PREF,
  TOKEN_FILENAME,
  tokenFileScope,
} from "#libs/shared/os-server-token/defines.ts";
import {
  assertTokenPath,
  TokenFileLifecycle,
} from "#libs/shared/os-server-token/lifecycle.ts";
import type { TokenDestination } from "#libs/shared/os-server-token/types.ts";

const WINDOWS_PROFILE_CONTRACT = "@floorp.app/private-profile-token;1";
interface WindowsProfileTokenNative {
  readonly protocolVersion: number;
  prepare(expectedProfileRoot: string): void;
  write(token: string): void;
  secure(): void;
  remove(): void;
}
let windowsOwner: WindowsProfileTokenNative | undefined;

/** Missing/unrecognized native code refuses before directory lookup or file IO.
 * There is deliberately no nsIFile/stat/IOUtils fallback for Windows profile mode. */
function requireWindowsOwner(): WindowsProfileTokenNative {
  if (windowsOwner) return windowsOwner;
  try {
    const interfaces = Ci as unknown as Record<string, typeof Ci.nsISupports>;
    const iid = interfaces.nsIFloorpPrivateProfileToken;
    if (!iid) throw new Error();
    const factory = Cc[WINDOWS_PROFILE_CONTRACT];
    if (!factory) throw new Error();
    const owner = factory.createInstance(
      iid,
    ) as unknown as WindowsProfileTokenNative;
    if (
      owner.protocolVersion !== 1 ||
      ![owner.prepare, owner.write, owner.secure, owner.remove].every(
        (method) => typeof method === "function",
      )
    ) throw new Error();
    windowsOwner = owner;
    return owner;
  } catch {
    throw new Error(
      "Native profile token advertisement unavailable on Windows",
    );
  }
}
function windowsTarget(target: TokenDestination): boolean {
  return target.profileScoped && Services.appinfo.OS === "WINNT";
}

function destination(): TokenDestination | null {
  // Resolve at publication time, not module initialization: the startup marker
  // may not exist yet. Explicit opt-in with an unknown mode refuses publication.
  const scope = tokenFileScope(
    Services.prefs.getBoolPref(PROFILE_TOKEN_PREF, false),
    Services.prefs.getStringPref("nora.startup.mode", ""),
  );
  if (!scope) return null;
  // Capability/ABI checks precede dirsvc/IO. Old Runtime artifacts remain
  // fail-closed until the source patch is actually compiled into the Runtime.
  if (scope === "profile" && Services.appinfo.OS === "WINNT") {
    requireWindowsOwner();
  }
  const root =
    Services.dirsvc.get(scope === "profile" ? "ProfD" : "Home", Ci.nsIFile)
      .path;
  const directory = PathUtils.join(
    root,
    scope === "profile" ? PROFILE_TOKEN_DIRECTORY : ".floorp",
  );
  return {
    directory,
    path: PathUtils.join(directory, TOKEN_FILENAME),
    profileScoped: scope === "profile",
  };
}

function guardPath(target: TokenDestination): void {
  assertTokenPath(target, {
    platform: () => Services.appinfo.OS,
    inspect(path) {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(path);
      try {
        // isSymlink uses lstat on Unix, including dangling links. Windows is
        // rejected above: its nsIFile implementation returns false for links.
        if (file.isSymlink()) return "link";
      } catch (error) {
        if (
          error instanceof Ci.nsIException &&
          error.result === Cr.NS_ERROR_FILE_NOT_FOUND
        ) return "missing";
        throw error;
      }
      if (!file.exists()) return "missing";
      if (file.isDirectory()) return "directory";
      if (file.isFile()) return "file";
      throw new Error("Unsupported token location node");
    },
    parent(path) {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(path);
      return file.parent?.path ?? null;
    },
  });
}

// Import this adapter through its canonical resource URL in every consumer so
// Gecko supplies one lifecycle owner, irrespective of bundler entry splitting.
export const tokenFile = new TokenFileLifecycle({
  async prepare(target) {
    if (windowsTarget(target)) {
      requireWindowsOwner().prepare(PathUtils.parent(target.directory));
      return;
    }
    guardPath(target);
    await IOUtils.makeDirectory(target.directory, {
      ignoreExisting: true,
      permissions: 0o700,
    });
    // Only our dedicated profile directory is tightened; do not chmod the
    // existing compatibility directory owned by other product integrations.
    guardPath(target);
    if (target.profileScoped) {
      await IOUtils.setPermissions(target.directory, 0o700);
    }
  },
  async write(target, token) {
    if (windowsTarget(target)) {
      requireWindowsOwner().write(token);
      return;
    }
    guardPath(target);
    // Create only an empty file before tightening permissions. This also fixes
    // permissive pre-existing compatibility files before secret bytes are written.
    if (!await IOUtils.exists(target.path)) {
      guardPath(target);
      await IOUtils.write(target.path, new Uint8Array(), { mode: "create" });
    }
    guardPath(target);
    await IOUtils.setPermissions(target.path, 0o600);
    guardPath(target);
    await IOUtils.write(target.path, new TextEncoder().encode(token), {
      mode: "overwrite",
    });
  },
  async secure(target) {
    if (windowsTarget(target)) {
      requireWindowsOwner().secure();
      return;
    }
    guardPath(target);
    await IOUtils.setPermissions(target.path, 0o600);
  },
  async remove(target) {
    if (windowsTarget(target)) {
      requireWindowsOwner().remove();
      return;
    }
    guardPath(target);
    await IOUtils.remove(target.path, { ignoreAbsent: true });
  },
});

export async function publishTokenFile(token: string): Promise<void> {
  // Keep this function async and await the publication promise: a rejected
  // publish and a synchronous destination() refusal (unsupported opt-in on
  // Windows, before any dirsvc/IO) must both surface as promise rejections.
  let target: TokenDestination | null;
  try {
    target = destination();
  } catch (error) {
    // A resolution refusal must also withdraw only our own captured
    // advertisement and cancel queued/in-flight publications before it
    // rejects. Otherwise a legacy Home token written before the opt-in
    // switch survives the refusal. This runs only on resolution failure:
    // lifecycle publish errors are already cleaned up by publish().
    await tokenFile.cleanup();
    throw error;
  }
  return await tokenFile.publish(target, token);
}

/** Reset must preserve the live MCP credential when OS alone is disabled. */
export function cleanupTokenFile(): Promise<void> {
  return tokenFile.cleanup();
}
