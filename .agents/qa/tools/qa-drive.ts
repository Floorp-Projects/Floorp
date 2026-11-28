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
//   status   [--os-port N] [--marionette-port N] [--token T|--profile <dir>]
//   os       <METHOD> <path> [--data <json>] [--os-port N] [--token T|--profile <dir>]
//   os-token --profile <dir>
//   eval     <js> [--marionette-port N] [--context chrome|content] [--async] [--timeout ms]
//   shot     [--out <path>] [--marionette-port N]
//   wait     [--os-port N] [--timeout <sec>] [--token T|--profile <dir>]
//
// All output is JSON on stdout so calling agents can parse it reliably.
// Errors go to stderr and produce a non-zero exit code.

import { MarionetteClient } from "../../../tools/src/browser_connector.ts";

type JsonValue = Record<string, unknown> | string | number | boolean | null;

function out(value: JsonValue): void {
  // Always JSON-encode, including strings, so callers can parse stdout reliably
  // (a bare string like "true" would otherwise be misread as another type).
  console.log(JSON.stringify(value));
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

// Switches that never take a value. Without this list, `eval --async 'script'`
// would swallow the script as the value of --async.
const BOOLEAN_OPTS = new Set(["async", "system-access", "no-marionette"]);

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
      if (BOOLEAN_OPTS.has(key)) {
        opts[key] = true;
        continue;
      }
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

function extractToken(prefsText: string): string | undefined {
  const m = prefsText.match(/user_pref\("floorp\.os\.server\.token",\s*"([^"]+)"/);
  return m?.[1];
}

async function readTokenFromProfile(profileDir: string): Promise<string> {
  // user.js always carries the token for qa-drive-launched instances, even
  // before Firefox has flushed prefs.js.
  for (const name of ["prefs.js", "user.js"]) {
    try {
      const text = await Deno.readTextFile(`${profileDir}/${name}`);
      const token = extractToken(text);
      if (token) return token;
    } catch {
      // try next file
    }
  }
  fail(`os-server token not found in ${profileDir} — is floorp.os.enabled=true and the browser running?`);
}

// Best-effort token resolution for status/wait. Returns "" when unavailable;
// an empty token still proves liveness via a 401 response.
async function resolveOsToken(opts: Record<string, string | boolean>): Promise<string> {
  const explicit = strOpt(opts, "token");
  if (explicit) return explicit;
  const profile = strOpt(opts, "profile");
  if (profile) {
    for (const name of ["prefs.js", "user.js"]) {
      try {
        const text = await Deno.readTextFile(`${profile}/${name}`);
        const token = extractToken(text);
        if (token) return token;
      } catch {
        // try next file
      }
    }
  }
  return "";
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
//
// When a token is supplied, readiness means HTTP 200: a 401 proves *some* other
// instance owns the port but cannot serve our token. With an empty token a 401
// is accepted as unauthenticated liveness evidence only.
async function waitForOsServer(
  port: number,
  token: string,
  timeoutSec: number,
): Promise<boolean> {
  const deadline = Date.now() + timeoutSec * 1000;
  while (Date.now() < deadline) {
    try {
      const res = await osCall(port, token, "GET", "/tabs/list", undefined, 4000);
      if (res.status === 200) return true;
      if (!token && res.status === 401) return true;
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

async function writePrivate(path: string, content: string): Promise<void> {
  // QA profiles live under shared /tmp; the bearer token inside must not be
  // readable by other local users. mode only applies at creation, so remove
  // any pre-existing file first.
  try {
    await Deno.remove(path);
  } catch {
    // did not exist
  }
  await Deno.writeTextFile(path, content, { mode: 0o600 });
}

async function cmdLaunch(opts: Record<string, string | boolean>, multi: Record<string, string[]>): Promise<void> {
  const binary = strOpt(opts, "binary") ??
    "/Applications/Floorp.app/Contents/MacOS/floorp";
  let profile = strOpt(opts, "profile") ?? fail("--profile is required");
  await Deno.mkdir(profile, { recursive: true, mode: 0o700 });
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
  await writePrivate(`${profile}/user.js`, prefLines.join("\n") + "\n");

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

  // Record instance identity before waiting so stop can target this exact
  // process instead of matching command-line substrings.
  await writePrivate(
    `${profile}/qa-instance.json`,
    JSON.stringify({
      pid: child.pid,
      profile,
      osPort,
      marionettePort: useMarionette ? marionettePort : null,
      token,
      binary,
      launchedAt: new Date().toISOString(),
    }, null, 2) + "\n",
  );

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
      "os-server did not answer an authenticated request within 40s — the port may be owned by another instance, or floorp.os.enabled is not active";
  }
  out(result as JsonValue);
}

// -- stop -----------------------------------------------------------------------
//
// Never use pkill -f with a profile substring: it matches this CLI's own
// --profile argument (since "-profile" is a substring of "--profile") and any
// browser whose profile path merely shares a prefix. Instead the launch
// command records the child PID in qa-instance.json and stop verifies the
// process identity before signaling it.

async function processAlive(pid: number): Promise<string | undefined> {
  try {
    const res = await new Deno.Command("/bin/ps", {
      args: ["-p", String(pid), "-o", "command="],
      stdout: "piped",
      stderr: "null",
    }).output();
    if (res.code !== 0) return undefined;
    const cmdline = new TextDecoder().decode(res.stdout).trim();
    return cmdline || undefined;
  } catch {
    return undefined;
  }
}

async function cmdStop(opts: Record<string, string | boolean>): Promise<void> {
  let profile = strOpt(opts, "profile") ?? fail("--profile is required");
  try {
    profile = await Deno.realPath(profile);
  } catch {
    // keep literal path
  }
  const metaPath = `${profile}/qa-instance.json`;
  let meta: { pid?: number; profile?: string };
  try {
    meta = JSON.parse(await Deno.readTextFile(metaPath));
  } catch {
    fail(
      `no qa-instance.json in ${profile} — refusing to guess processes. ` +
        `Only instances launched by "qa-drive launch" can be stopped safely.`,
    );
  }
  const pid = meta.pid;
  if (typeof pid !== "number" || pid <= 0) {
    fail(`qa-instance.json in ${profile} has no valid pid`);
  }

  const cmdline = await processAlive(pid);
  if (cmdline === undefined) {
    try {
      await Deno.remove(metaPath);
    } catch {
      // best effort
    }
    out({ stopped: false, alreadyExited: true, pid, profile });
    return;
  }
  const metaProfile = typeof meta.profile === "string" ? meta.profile : profile;
  const verified = cmdline.includes("-profile") &&
    cmdline.includes(metaProfile) &&
    /floorp/i.test(cmdline);
  if (!verified) {
    fail(
      `pid ${pid} does not look like the Floorp instance for ${metaProfile} — refusing to signal it`,
      { cmdline },
    );
  }

  try {
    Deno.kill(pid, "SIGTERM");
  } catch (e) {
    fail(`failed to signal pid ${pid}: ${(e as Error).message}`);
  }
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    if ((await processAlive(pid)) === undefined) break;
    await new Promise((r) => setTimeout(r, 300));
  }
  let killed = false;
  if ((await processAlive(pid)) !== undefined) {
    try {
      Deno.kill(pid, "SIGKILL");
      killed = true;
    } catch {
      // already gone
    }
    await new Promise((r) => setTimeout(r, 500));
  }
  const gone = (await processAlive(pid)) === undefined;
  if (gone) {
    try {
      await Deno.remove(metaPath);
    } catch {
      // best effort
    }
  }
  out({ stopped: gone, pid, sigkill: killed, profile });
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
    const token = await resolveOsToken(opts);
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
      if (!profile) fail("os: provide --token or --profile (to read token from the profile)");
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
    const timeoutMs = numOpt(opts, "timeout", 30000);
    const client = await MarionetteClient.connect(marionettePort);
    try {
      await client.setContext(context);
      // The session's own script timeout defaults to ~30s in the browser;
      // without this, --timeout only extends the client-side socket deadline.
      await client.send("WebDriver:SetTimeouts", { script: timeoutMs });
      if (opts.async === true) {
        // WebDriver:ExecuteAsyncScript — the script body's last argument is a
        // finish callback. The script is wrapped so the user writes a body that
        // ends by calling resolve(value) or reject(error).
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
    // Real HTTP readiness — a bare TCP probe can wedge the os-server on some
    // builds (see harness.md) and does not prove the API answers.
    const token = await resolveOsToken(opts);
    const ready = await waitForOsServer(osPort, token, timeoutSec);
    out({ osPort, ready });
    if (!ready) Deno.exit(2);
    break;
  }

  default:
    fail(
      `unknown command: ${cmd ?? "(none)"}. Expected: launch|stop|status|os|os-token|eval|shot|wait`,
    );
}

// MarionetteClient leaves socket-read timers referenced even after close();
// exiting explicitly keeps successful eval/shot commands from stalling ~30s.
Deno.exit(0);
