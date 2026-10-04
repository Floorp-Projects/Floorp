function removeUnsafeControlCharacters(value: string): string {
  let sanitized = "";
  for (const char of value) {
    const code = char.charCodeAt(0);
    const isUnsafeControl = (code >= 0 && code <= 8) ||
      code === 11 ||
      code === 12 ||
      (code >= 14 && code <= 31) ||
      code === 127;
    if (!isUnsafeControl) {
      sanitized += char;
    }
  }
  return sanitized;
}

/**
 * Sanitizes a single Desktop Entry value so manifest-controlled text cannot
 * inject additional keys by including line breaks or control characters.
 */
export function sanitizeDesktopEntryValue(value: string): string {
  return removeUnsafeControlCharacters(value.replace(/[\r\n]+/g, " "))
    .replace(/\\/g, "\\\\")
    .trim();
}

/** Resolve the Linux user data directory, ignoring relative XDG_DATA_HOME. */
export function resolveLinuxDataHome(
  homeDir: string,
  xdgDataHome: string,
): string {
  return xdgDataHome.startsWith("/")
    ? xdgDataHome
    : `${homeDir.replace(/\/+$/g, "")}/.local/share`;
}

/** Encode one argument for the Desktop Entry Exec key. */
export function escapeDesktopExecToken(token: string): string {
  // Desktop Entry field codes are expanded even though Exec is not a shell.
  // deno-lint-ignore no-control-regex
  if (/[\x00-\x1f\x7f]/.test(token)) {
    throw new Error("Control character in desktop Exec argument");
  }

  const escapedPercent = token.replaceAll("%", "%%");
  if (token.length > 0 && !/[\s"'\\><~|&;$*?#()`]/.test(token)) {
    return escapedPercent;
  }

  // The Desktop Entry value parser consumes one backslash layer before the
  // argument quoting layer, so a literal backslash needs four in the file.
  const escapedQuoted = escapedPercent.replaceAll("\\", "\\\\\\\\")
    .replace(/["`$]/g, (character) => `\\\\${character}`);
  return `"${escapedQuoted}"`;
}

/** Only recognize a Floorp-generated launcher for this app and exact command. */
export function isOwnedLinuxDesktopEntry(
  contents: string,
  appId: string,
  expectedCommands: readonly string[],
): boolean {
  const lines = contents.split(/\r?\n/);
  const sections = lines.filter((line) => line.startsWith("["));
  const ids = lines.filter((line) => line.startsWith("X-Floorp-Id="));
  const commands = lines.filter((line) => line.startsWith("Exec="));
  return sections.length === 1 && sections[0] === "[Desktop Entry]" &&
    ids.length === 1 &&
    ids[0] === `X-Floorp-Id=${sanitizeDesktopEntryValue(appId)}` &&
    commands.length === 1 &&
    expectedCommands.some((command) => commands[0] === `Exec=${command}`);
}
