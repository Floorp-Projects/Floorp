import { assertEquals } from "@std/assert";
import { sanitizeDesktopEntryValue } from "#libs/pwa/desktopEntry.ts";

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
