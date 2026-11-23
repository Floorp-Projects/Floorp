// SPDX-License-Identifier: MPL-2.0

export const PROFILE_TOKEN_PREF = "nora.dev.os_server.profile_token_file";
export const PROFILE_TOKEN_DIRECTORY = "floorp-os-server";
export const TOKEN_FILENAME = "os-server-token";

/** An explicit isolation request fails closed until startup establishes dev/test. */
export function tokenFileScope(
  optIn: boolean,
  startupMode: string,
): "profile" | "legacy" | null {
  if (!optIn) return "legacy";
  return startupMode === "dev" || startupMode === "test" ? "profile" : null;
}
