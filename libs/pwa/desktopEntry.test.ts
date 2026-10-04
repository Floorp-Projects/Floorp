import { assertEquals, assertThrows } from "@std/assert";
import {
  escapeDesktopExecToken,
  isOwnedLinuxDesktopEntry,
  resolveLinuxDataHome,
  sanitizeDesktopEntryValue,
} from "#libs/pwa/desktopEntry.ts";

Deno.test("sanitizeDesktopEntryValue prevents key injection through line breaks", () => {
  assertEquals(
    sanitizeDesktopEntryValue("Safe Name\nExec=/bin/sh\r\nX-Test=value"),
    "Safe Name Exec=/bin/sh X-Test=value",
  );
});

Deno.test("sanitizeDesktopEntryValue removes control characters", () => {
  assertEquals(
    sanitizeDesktopEntryValue("App\u0000Name\u0007"),
    "AppName",
  );
});

Deno.test("sanitizeDesktopEntryValue escapes backslashes", () => {
  assertEquals(
    sanitizeDesktopEntryValue(String.raw`C:\Users\Name`),
    String.raw`C:\\Users\\Name`,
  );
});

Deno.test("resolveLinuxDataHome honors absolute XDG paths", () => {
  assertEquals(resolveLinuxDataHome("/home/user", "/data/user"), "/data/user");
  assertEquals(
    resolveLinuxDataHome("/home/user", "relative/path"),
    "/home/user/.local/share",
  );
  assertEquals(
    resolveLinuxDataHome("/home/user", ""),
    "/home/user/.local/share",
  );
});

Deno.test("escapeDesktopExecToken preserves literal percent and quoted paths", () => {
  assertEquals(escapeDesktopExecToken("/opt/100%floorp"), "/opt/100%%floorp");
  assertEquals(
    escapeDesktopExecToken("/home/user/My Profile 100%"),
    '"/home/user/My Profile 100%%"',
  );
  assertEquals(escapeDesktopExecToken(""), '""');
});

Deno.test("escapeDesktopExecToken handles reserved characters and rejects controls", () => {
  assertEquals(escapeDesktopExecToken("a$b"), '"a\\\\$b"');
  assertEquals(escapeDesktopExecToken("a\\b"), '"a\\\\\\\\b"');
  assertEquals(escapeDesktopExecToken('a"b'), '"a\\\\"b"');
  assertEquals(escapeDesktopExecToken("a`b"), '"a\\\\`b"');
  for (
    const reserved of [
      " ",
      "'",
      ">",
      "<",
      "~",
      "|",
      "&",
      ";",
      "*",
      "?",
      "#",
      "(",
      ")",
    ]
  ) {
    assertEquals(escapeDesktopExecToken(`a${reserved}b`), `"a${reserved}b"`);
  }
  for (const control of ["\0", "\n", "\r", "\t", "\x1b", "\x7f"]) {
    assertThrows(() => escapeDesktopExecToken(`a${control}b`), Error);
  }
});

Deno.test("escapeDesktopExecToken escapes field codes without changing argument boundaries", () => {
  assertEquals(escapeDesktopExecToken("%f%U%%"), "%%f%%U%%%%");
  assertEquals(escapeDesktopExecToken("profile with %u"), '"profile with %%u"');
  assertEquals(escapeDesktopExecToken("日本語"), "日本語");
  assertEquals(escapeDesktopExecToken("--start-ssb"), "--start-ssb");
  assertEquals(
    escapeDesktopExecToken("https://example.com/"),
    "https://example.com/",
  );
});

Deno.test("legacy desktop migration requires this app and exact profile-bound command", () => {
  const command = 'floorp --profile "/home/user/profile" --start-ssb app';
  const entry = `[Desktop Entry]\nX-Floorp-Id=app\nExec=${command}\n`;
  assertEquals(isOwnedLinuxDesktopEntry(entry, "app", [command]), true);
  assertEquals(isOwnedLinuxDesktopEntry(entry, "other-app", [command]), false);
  assertEquals(
    isOwnedLinuxDesktopEntry(entry, "app", [
      command.replace('profile"', 'other"'),
    ]),
    false,
  );
  for (const extra of ["[Other]\n", "Exec=other\n", "X-Floorp-Id=app\n"]) {
    assertEquals(
      isOwnedLinuxDesktopEntry(entry + extra, "app", [command]),
      false,
    );
  }
});
