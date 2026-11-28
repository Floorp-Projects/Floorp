#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0
//
// qa-drive.ts — Floorp QA harness CLI.
//
// Drives packaged or staged Floorp builds for QA:
//   - launch/stop browser instances on disposable profiles
//   - os-server HTTP API calls (loopback automation API built into Floorp)
//   - Marionette chrome/content eval + screenshots (dev/test builds)
//
// Usage:
//   deno run -A .agents/qa/tools/qa-drive.ts <command> [options]
//
// Commands:
//   launch   --binary <path> --profile <dir> [--os-port N] [--marionette-port N]
//            [--token T] [--pref name=value]... [--system-access] [--no-marionette]
//   stop     --profile <dir>
//   status   [--os-port N] [--marionette-port N]
//   os       <METHOD> <path> [--data <json>] [--os-port N] [--token T|--profile <dir>]
//   os-token --profile <dir>
//   eval     <js> [--marionette-port N] [--context chrome|content]
//   shot     [--out <path>] [--marionette-port N]
//   wait     [--os-port N] [--marionette-port N] [--timeout <sec>]
//
// All output is JSON on stdout so calling agents can parse it reliably.
// Errors go to stderr and produce a non-zero exit code.

import { MarionetteClient } from "../../../tools/src/browser_connector.ts";

type JsonValue = Record<string, unknown> | string | number | boolean | null;

function out(value: JsonValue): void {
  console.log(typeof value === "string" ? value : JSON.stringify(value));
}

function fail(message: string, detail?: unknown): never {
  console.error(`[qa-drive] ERROR: ${message}`);
  if (detail !== undefined) {
    console.error(
      typeof detail === "string" ? detail : JSON.stringify(detail, null, 2),
    );
  }
  Deno.exit(1);
}

// -- arg parsing ---------------------------------------------------------------

function parseArgs(argv: string[]): {
  cmd: string | undefined;
  positional: string[];
  opts: Record<string, string | boolean>;
  multi: Record<string, string[]>;
} {
  const positional: string[] = [];
  const opts: Record<string, string | boolean> = {};
  const multi: Record<string, string[]> = {};
  let cmd: string | undefined;
  let i = 0;
  if (argv.length > 0 && !argv[0].startsWith("--")) {
    cmd = argv[0];
    i = 1;
  }
  for (; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const key = a.slice(2);
      const next = argv[i + 1];
      if (next !== undefined && !next.startsWith("--")) {
        if (key === "pref") {
          (multi.pref ??= []).push(next);
        } else {
          opts[key] = next;
        }
        i++;
      } else {
        opts[key] = true;
      }
    } else {
      positional.push(a);
    }
  }
  return { cmd, positional, opts, multi };
}

function numOpt(opts: Record<string, string | boolean>, key: string, dflt: number): number {
  const v = opts[key];
  if (v === undefined) return dflt;
  const n = parseInt(String(v), 10);
  if (Number.isNaN(n)) fail(`--${key} must be a number, got: ${v}`);
  return n;
}

function strOpt(opts: Record<string, string | boolean>, key: string): string | undefined {
  const v = opts[key];
  return v === undefined || v === true ? undefined : String(v);
}

// -- os-server helpers ----------------------------------------------------------

async function readTokenFromProfile(profileDir: string): Promise<string> {
  try {
    const prefs = await Deno.readTextFile(`${profileDir}/prefs.js`);
    const m = prefs.match(/user_pref\("floorp\.os\.server\.token",\s*"([^"]+)"/);
    if (m) return m[1];
  } catch {
    // fall through
  }
  fail(`os-server token not found in ${profileDir}/prefs.js — is floorp.os.enabled=true and the browser running?`);
}

async function osCall(
  port: number,
  token: string,
  method: string,
  path: string,
  data?: unknown,
  timeoutMs = 30_000,
): Promise<{ status: number; body: unknown }> {
  const res = await fetch(`http://127.0.0.1:${port}${path}`, {
    method,
    headers: {
      Authorization: `Bearer ${token}`,
      "Content-Type": "application/json",
    },
    body: data === undefined ? undefined : JSON.stringify(data),
    signal: AbortSignal.timeout(timeoutMs),
  });
  const text = await res.text();
  let body: unknown = text;
  try {
    body = JSON.parse(text);
  } catch {
    // leave as text
  }
  return { status: res.status, body };
}

// NOTE: never probe the os-server with bare TCP connect/close — each probe is an
// incomplete HTTP request on the server side and produces socket errors / stuck
// handlers on some versions. Readiness is checked with real HTTP GETs instead.
async function waitForOsServer(
  port: number,
  token: string,
  timeoutSec: number,
): Promise<boolean> {
  const deadline = Date.now() + timeoutSec * 1000;
  while (Date.now() < deadline) {
    try {
      const res = await osCall(port, token, "GET", "/tabs/list", undefined, 4000);
      if (res.status === 200 || res.status === 401) return true;
    } catch {
      // not up yet
    }
    await new Promise((r) => setTimeout(r, 700));
  }
  return false;
}

async function waitForTcp(port: number, timeoutSec: number): Promise<boolean> {
  const deadline = Date.now() + timeoutSec * 1000;
  while (Date.now() < deadline) {
    try {
      const conn = await Deno.connect({ hostname: "127.0.0.1", port });
      conn.close();
      return true;
    } catch {
      await new Promise((r) => setTimeout(r, 500));
    }
  }
  return false;
}

// -- launch ---------------------------------------------------------------------

function typedPrefValue(raw: string): string {
  if (raw === "true" || raw === "false") return raw;
  if (/^-?\d+$/.test(raw)) return raw;
  return JSON.stringify(raw);
}

async function cmdLaunch(opts: Record<string, string | boolean>, multi: Record<string, string[]>): Promise<void> {
  const binary = strOpt(opts, "binary") ??
    "/Applications/Floorp.app/Contents/MacOS/floorp";
  let profile = strOpt(opts, "profile") ?? fail("--profile is required");
  await Deno.mkdir(profile, { recursive: true });
  // Normalize symlinks (macOS /tmp -> /private/tmp) so the process command line
  // matches the path used for later stop/status operations.
  try {
    profile = await Deno.realPath(profile);
  } catch {
    // keep literal path
  }
  const osPort = numOpt(opts, "os-port", 58262);
  const marionettePort = numOpt(opts, "marionette-port", 28292);
  const token = strOpt(opts, "token") ?? crypto.randomUUID();
  const useMarionette = opts["no-marionette"] !== true;
  const systemAccess = opts["system-access"] === true;

  const prefLines: string[] = [
    `user_pref("floorp.os.enabled", true);`,
    `user_pref("floorp.os.server.port", ${osPort});`,
    `user_pref("floorp.os.server.token", "${token}");`,
    `user_pref("browser.shell.checkDefaultBrowser", false);`,
    `user_pref("browser.sessionstore.resume_from_crash", false);`,
    `user_pref("datareporting.policy.dataSubmissionEnabled", false);`,
    `user_pref("toolkit.telemetry.reportingpolicy.firstRun", false);`,
  ];
  if (useMarionette) {
    prefLines.push(
      `user_pref("marionette.enabled", true);`,
      `user_pref("marionette.port", ${marionettePort});`,
    );
  }
  for (const p of multi.pref ?? []) {
    const eq = p.indexOf("=");
    if (eq < 1) fail(`--pref must be name=value, got: ${p}`);
    prefLines.push(`user_pref(${JSON.stringify(p.slice(0, eq))}, ${typedPrefValue(p.slice(eq + 1))});`);
  }
  await Deno.writeTextFile(`${profile}/user.js`, prefLines.join("\n") + "\n");

  const args = [
    "--no-remote",
    "-profile",
    profile,
  ];
  if (useMarionette) {
    args.push("--marionette");
    if (systemAccess) args.push("--remote-allow-system-access");
  }

  const env: Record<string, string> = {
    MOZ_CRASHREPORTER_DISABLE: "1",
  };
  if (systemAccess) env.MOZ_REMOTE_ALLOW_SYSTEM_ACCESS = "1";

  const command = new Deno.Command(binary, {
    args,
    env,
    stdout: "null",
    stderr: "null",
    stdin: "null",
  });
  const child = command.spawn();
  // Detach: unref so this CLI can exit while the browser keeps running.
  child.unref();

  const osReady = await waitForOsServer(osPort, token, 40);
  const result: Record<string, unknown> = {
    launched: true,
    pid: child.pid,
    profile,
    osPort,
    osServerReady: osReady,
    token,
  };
  if (useMarionette) {
    result.marionettePort = marionettePort;
    result.marionetteListening = await waitForTcp(marionettePort, 5);
  }
  if (!osReady) {
    result.warning =
      "os-server port did not open within 30s — check floorp.os.enabled pref and binary path";
  }
  out(result as JsonValue);
}

async function cmdStop(opts: Record<string, string | boolean>): Promise<void> {
  const profile = strOpt(opts, "profile") ?? fail("--profile is required");
  // macOS resolves /tmp to /private/tmp in the process command line, so match
  // both the literal and resolved variants of the unique profile path.
  const variants = [profile];
  if (profile.startsWith("/tmp/")) variants.push(`/private${profile}`);
  let stopped = false;
  for (const variant of variants) {
    const proc = new Deno.Command("pkill", {
      args: ["-f", "--", `-profile ${variant}`],
      stdout: "null",
      stderr: "null",
    });
    const status = await proc.output();
    stopped = stopped || status.code === 0;
  }
  out({ stopped, profile });
}

// -- main -----------------------------------------------------------------------

const { cmd, positional, opts, multi } = parseArgs(Deno.args);

switch (cmd) {
  case "launch":
    await cmdLaunch(opts, multi);
    break;

  case "stop":
    await cmdStop(opts);
    break;

  case "status": {
    const osPort = numOpt(opts, "os-port", 58262);
    const marionettePort = numOpt(opts, "marionette-port", 28292);
    let token = strOpt(opts, "token") ?? "";
    if (!token) {
      const profile = strOpt(opts, "profile");
      if (profile) {
        try {
          const prefs = await Deno.readTextFile(`${profile}/prefs.js`);
          const m = prefs.match(/user_pref\("floorp\.os\.server\.token",\s*"([^"]+)"/);
          if (m) token = m[1];
        } catch {
          // no token available — a 401 still proves the server is up
        }
      }
    }
    // os-server readiness is only meaningful with a token; a 401 still proves the server is up.
    const osOpen = await waitForOsServer(osPort, token, 4);
    const marionetteOpen = await waitForTcp(marionettePort, 1);
    out({ osPort, osOpen, marionettePort, marionetteOpen });
    if (!osOpen) Deno.exit(2);
    break;
  }

  case "os": {
    const [method, path] = positional;
    if (!method || !path) fail("usage: os <METHOD> <path> [--data json] [--os-port N] [--token T|--profile dir]");
    const osPort = numOpt(opts, "os-port", 58262);
    let token = strOpt(opts, "token");
    if (!token) {
      const profile = strOpt(opts, "profile");
      if (!profile) fail("os: provide --token or --profile (to read token from prefs.js)");
      token = await readTokenFromProfile(profile);
    }
    let data: unknown;
    const rawData = strOpt(opts, "data");
    if (rawData !== undefined) {
      try {
        data = JSON.parse(rawData);
      } catch {
        fail(`--data is not valid JSON: ${rawData}`);
      }
    }
    const res = await osCall(osPort, token, method.toUpperCase(), path, data);
    out({ status: res.status, body: res.body } as JsonValue);
    if (res.status >= 400) Deno.exit(3);
    break;
  }

  case "os-token": {
    const profile = strOpt(opts, "profile") ?? fail("--profile is required");
    out({ token: await readTokenFromProfile(profile) });
    break;
  }

  case "eval": {
    const script = positional.join(" ");
    if (!script) fail("usage: eval <js> [--marionette-port N] [--context chrome|content] [--async] [--timeout ms]");
    const marionettePort = numOpt(opts, "marionette-port", 28292);
    const context = strOpt(opts, "context") === "content" ? "content" : "chrome";
    const client = await MarionetteClient.connect(marionettePort);
    try {
      await client.setContext(context);
      if (opts.async === true) {
        // WebDriver:ExecuteAsyncScript — the script body's last argument is a
        // finish callback. The script is wrapped so the user writes a body that
        // ends by calling resolve(value) or reject(error).
        const timeoutMs = numOpt(opts, "timeout", 30000);
        const wrapped =
          `const resolve = arguments[arguments.length - 1];\n` +
          `const reject = (e) => resolve({__qaError: String((e && e.message) || e)});\n` +
          script;
        const result = await client.sendWithTimeout(
          "WebDriver:ExecuteAsyncScript",
          { script: wrapped, args: [], newSandbox: true },
          timeoutMs + 5000,
        );
        out(result?.value as JsonValue);
      } else {
        const result = await client.executeScript(`return (${script})`);
        out(result as JsonValue);
      }
    } finally {
      await client.close();
    }
    break;
  }

  case "shot": {
    const marionettePort = numOpt(opts, "marionette-port", 28292);
    const outPath = strOpt(opts, "out") ?? `qa-shot-${Date.now()}.png`;
    const client = await MarionetteClient.connect(marionettePort);
    try {
      await client.setContext("chrome");
      const bytes = await client.takeScreenshot({ full: true });
      await Deno.writeFile(outPath, bytes);
      out({ screenshot: outPath });
    } finally {
      await client.close();
    }
    break;
  }

  case "wait": {
    const timeoutSec = numOpt(opts, "timeout", 30);
    const osPort = numOpt(opts, "os-port", 58262);
    const ready = await waitForTcp(osPort, timeoutSec);
    out({ osPort, ready });
    if (!ready) Deno.exit(2);
    break;
  }

  default:
    fail(
      `unknown command: ${cmd ?? "(none)"}. Expected: launch|stop|status|os|os-token|eval|shot|wait`,
    );
}
