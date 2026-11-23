# FDH v1.0 AC-07 Windows native token follow-up

On 2026-11-21 UTC, a Windows dev browser started its OS-Server on
`127.0.0.1:5375` while a sentinel held the installed-browser port `58261`. The
browser published a token in its dev profile and authenticated `GET /health`
returned `200 {"status":"ok"}`. A chrome feature edit reached the same browser
window through HMR. These results used an explicitly prepared Debug Runtime. The
checkout's lock-pinned Release Runtime remains outside the authenticated result.
See the [previous Windows AC-07 run](fdh-ac07-windows/README.md).

## Environment and Runtime

- Windows Server 2022 x64, build `10.0.20348.5622`. Deno 2.9.6 and Python
  3.13.12.
- The Floorp checkout matched `origin/main` at
  [`b2b423cbc21d3356f7574533fad1ee020b451d69`](https://github.com/Floorp-Projects/Floorp/commit/b2b423cbc21d3356f7574533fad1ee020b451d69).
  It included the
  [KAN-135 port fix](https://github.com/Floorp-Projects/Floorp/pull/3167).
- Debug Runtime `160.0.1`, build ID `20261114215908`. The Windows x64 artifact's
  `runtime-source-provenance.json` identified Runtime commit
  `de88fb8242382ec1a0392c4c6de3d65e09f6aa88` from
  [Runtime PR #115](https://github.com/Floorp-Projects/Floorp-Runtime/pull/115).
  The native contract reported ABI 1 in the live browser.
- The
  [Actions run](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/43645647081)
  artifact was downloaded via
  [nightly.link](https://nightly.link/Floorp-Projects/Floorp-Runtime/actions/runs/43645647081/floorp-windows-x86_64-moz-artifact.zip).
  The inner `floorp-windows-x86_64-moz-artifact.zip` was 190337065 bytes with
  SHA-256 `11fa924d1a9c427749efc74d8c594b46c5223c4e4049de7adec1ae72d27dbc0a`.

## Setup and launch

Run commands from the checkout root in Git Bash. The VM had no installed Floorp
process or existing listener on `58261`. Start the sentinel in a separate shell
and leave it running through browser teardown.

```bash
deno eval --ext=ts 'Deno.serve({ hostname: "127.0.0.1", port: 58261 }, () => new Response("fdh-installed-port-sentinel", { status: 503 }));'
```

Check the response from the checkout shell.

```bash
curl.exe --noproxy '*' --max-time 5 -sS -i http://127.0.0.1:58261/health
```

The first attempt selected `FLOORP_DEV_RUNTIME_DISTRIBUTION=release` and used
`deno run -A tools/runtime-lock/runtime_lock_cli.ts install-native`, which
confirmed the existing Release Runtime (build `20261112140843`). With task ID
`fdh-ac07-windows-followup` and offset 200, `fdh task prepare` reported
`runtime:missing` and skipped applying the BrowserGlue startup hook.
`fdh dev up` reached Marionette on `5380`, with no listener on `5375`.
`fdh dev down` stopped 43 processes. The
[previous Windows run](fdh-ac07-windows/README.md) covers the same locked
Release Runtime with the BrowserGlue hook applied and records its token
publication failure.

For the Debug run, move the old Runtime and browser profile aside before
preparing the task. The source marker below describes the downloaded archive.
FDH created the prepared Runtime receipt and applied the BrowserGlue hook under
`_dist/bin`. The run left `FLOORP_DEV_RUNTIME_DISTRIBUTION` unset so the
initializer selected the explicit Debug Runtime receipt.

```bash
proof=_dist/fdh/windows-native-proof
mkdir -p "$proof"
curl.exe -fL --silent --show-error --retry 2 \
  https://nightly.link/Floorp-Projects/Floorp-Runtime/actions/runs/43645647081/floorp-windows-x86_64-moz-artifact.zip \
  -o "$proof/actions-artifact.zip"
python - <<'PY'
import hashlib, json, pathlib, zipfile

proof = pathlib.Path("_dist/fdh/windows-native-proof")
name = "floorp-windows-x86_64-moz-artifact.zip"
with zipfile.ZipFile(proof / "actions-artifact.zip") as outer:
    outer.extract(name, proof)
    provenance = json.loads(outer.read("runtime-source-provenance.json"))
assert provenance["runtime_commit"] == "de88fb8242382ec1a0392c4c6de3d65e09f6aa88"
archive = proof / name
with archive.open("rb") as stream:
    digest = hashlib.file_digest(stream, "sha256").hexdigest()
assert digest == "11fa924d1a9c427749efc74d8c594b46c5223c4e4049de7adec1ae72d27dbc0a"
with zipfile.ZipFile(archive) as inner:
    inner.extractall(proof / "bin")
marker = dict(
    schemaVersion=1, distribution="debug", platform="windows", architecture="x86_64",
    artifactName=name, artifactSize=archive.stat().st_size, artifactSha256=digest,
    version="160.0.1", buildId="20261114215908",
)
(proof / "bin" / ".floorp-runtime-debug.json").write_text(json.dumps(marker) + "\n")
PY
mv _dist/bin "$proof/locked-bin"
mv _dist/profile/test "$proof/locked-profile"
unset FLOORP_DEV_RUNTIME_DISTRIBUTION
deno task fdh task start --task-id fdh-ac07-windows-native --worktree "$(pwd -W)" --port-offset 200
deno task fdh task prepare --task-id fdh-ac07-windows-native --runtime-source "$(pwd -W)/$proof/bin"
deno task fdh dev up --task-id fdh-ac07-windows-native
deno task fdh verify browser --task-id fdh-ac07-windows-native \
  --eval 'Services.prefs.getStringPref("nora.startup.loader","")' --expect loaded
```

## Port ownership and authenticated health

The startup log reported `[Floorp OS-Server] listening on 127.0.0.1:5375` during
automatic startup. The browser verification connected to Marionette and reported
`loaded`. The chrome probe read startup mode `dev`, offset `200`, OS-Server port
`5375`, ABI `1`, and a nonempty OS-Server token preference. It printed only a
boolean for the token. The dev profile published
`_dist/profile/test/floorp-os-server/os-server-token`. The legacy Home token
path did not exist.

```bash
netstat -ano -p TCP | grep -E ':(5375|5380|5381|5383|58261) .*LISTENING'
MSYS2_ARG_CONV_EXCL='*' tasklist.exe /FI 'PID eq 3848'
curl.exe --noproxy '*' --max-time 5 -sS -i http://127.0.0.1:58261/health
deno task dev-tool eval '(() => { const prefs = Services.prefs; const iid = Ci.nsIFloorpPrivateProfileToken; const contract = "@floorp.app/private-profile-token;1"; const abi = iid && Reflect.has(Cc,contract) ? Cc[contract].createInstance(iid).protocolVersion : null; const root = Services.wm.getMostRecentWindow("navigator:browser").document.documentElement; return JSON.stringify({pid:Services.appinfo.processID,os:Services.appinfo.OS,build:Services.appinfo.appBuildID,mode:prefs.getStringPref("nora.startup.mode",""),loader:prefs.getStringPref("nora.startup.loader",""),offset:prefs.getIntPref("nora.dev.port_offset",0),osPort:prefs.getIntPref("floorp.os.server.port",0),abi,issuedToken:Boolean(prefs.getStringPref("floorp.os.server.token","")),hmr:root.getAttribute("data-fdh-hmr-proof"),windowProbe:root.getAttribute("data-fdh-window-proof")}); })()' -c chrome
```

| Loopback port     | Listener during dev                                                                                                          |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| 5375 (5175 + 200) | OS-Server, `floorp.exe` PID 3848                                                                                             |
| 5380              | Marionette, same `floorp.exe` PID 3848                                                                                       |
| 5381              | Vite feature loader, PID 5808                                                                                                |
| 5383              | Vite settings, PID 8156                                                                                                      |
| 58261             | Sentinel, `deno.exe` PID 8576. Returned HTTP 503 and `fdh-installed-port-sentinel` before, during, and after the dev launch. |

The probe used the shared `tools/os-test` Python client and kept the bearer in
memory. The first request made as soon as the port bound returned 401 without a
bearer. The token file was still absent at that point. After publication, the
authenticated request returned HTTP 200 on its first attempt. Four subsequent
requests without a bearer returned the client's generic network, redirect, or
origin error before the fifth returned HTTP 401. An earlier request without a
bearer also returned that error. The run did not identify the cause of the
intermittent request failures.

```bash
FLOORP_OS_BASE_URL=http://127.0.0.1:5375 \
FLOORP_OS_TOKEN_FILE=_dist/profile/test/floorp-os-server/os-server-token \
python -u - <<'PY'
import os
import sys
from time import sleep

sys.path.insert(0, "tools/os-test")
from os_test_client import (
    OsTestClientError, create_os_test_client, resolve_base_url, resolve_token_file,
)

base = resolve_base_url(None, os.environ.get("FLOORP_OS_BASE_URL"))
token_file = resolve_token_file(None, os.environ.get("FLOORP_OS_TOKEN_FILE"))
for label, path, expected in (("profile bearer [REDACTED]", token_file, 200), ("no bearer", None, 401)):
    client = create_os_test_client(base, path)
    for attempt in range(1, 7):
        try:
            status, body = client.request("/health", timeout=3)
            print(f"{label} attempt {attempt}: {status} {body}")
            if status == expected and (status != 200 or body == {"status": "ok"}):
                break
        except OsTestClientError as error:
            print(f"{label} attempt {attempt}: {error}")
        sleep(0.25)
    else:
        raise SystemExit(f"{label}: expected HTTP {expected} was not observed")
PY
```

## Chrome HMR and teardown

With the dev browser running, the first eval marked the open browser window and
returned `null` for the feature's marker. The temporary edit added one line just
after `gFlexOrder.init();` in
`browser-features/chrome/common/flex-order/index.ts`.

```bash
deno task dev-tool eval '(() => { const root = Services.wm.getMostRecentWindow("navigator:browser").document.documentElement; root.setAttribute("data-fdh-window-proof", "original-window"); return root.getAttribute("data-fdh-hmr-proof"); })()' -c chrome
```

```ts
globalThis.document.documentElement.setAttribute(
  "data-fdh-hmr-proof",
  "windows-live-edit",
);
```

Rerun the chrome probe from the port and health check above, then run
`grep -i 'hmr update' logs/vite-main.log`. The first read after the edit still
showed `hmr: null`. The next read showed these fields in its JSON output:
`{"pid":3848,"hmr":"windows-live-edit","windowProbe":"original-window"}`. Vite
logged `hmr update /link-features-chrome/common/flex-order/index.ts` and the
browser logged
`[vite] hot updated: /link-features-chrome/common/flex-order/index.ts`. There
was no rebuild or browser restart between the reads. The feature file was
restored byte-for-byte before teardown.

```bash
deno task fdh dev down --task-id fdh-ac07-windows-native
netstat -ano -p TCP | grep -E ':(5375|5380|5381|5383|58261) .*LISTENING'
curl.exe --noproxy '*' --max-time 5 -sS -i http://127.0.0.1:58261/health
```

`fdh dev down` stopped 42 processes and the four dev listeners in the table were
gone. The dev profile token file remained after shutdown and was removed
manually. The sentinel was stopped separately, and port `58261` was then clear.
The original locked Runtime was restored.

This run covers coexistence with a simulated installed listener, successful
native token publication, one authenticated health response, and a chrome HMR
update on the explicit Debug build. Coexistence with a real installed Floorp and
authenticated health on the stock locked Release Runtime remain unverified.
