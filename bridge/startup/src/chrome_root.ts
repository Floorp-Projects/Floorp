// SPDX-License-Identifier: MPL-2.0

const HTTP_LOADER_PREF = "nora.dev.allow_http_loader";

async function importWithRetry(
  url: string,
  timeoutMs = 120_000,
  intervalMs = 500,
) {
  const startedAt = Date.now();
  let lastError: unknown;

  while (Date.now() - startedAt < timeoutMs) {
    try {
      return await import(`${url}?t=${Date.now()}`);
    } catch (error) {
      lastError = error;
      await new Promise((resolve) => setTimeout(resolve, intervalMs));
    }
  }

  throw lastError ?? new Error(`Failed to import module: ${url}`);
}

function setStartupMarker(key: string, value: string) {
  try {
    Services.prefs.setStringPref(key, value);
  } catch {
    // Diagnostics should never block startup flow.
  }
}

function assertHttpLoaderAllowed(): void {
  try {
    const allowed = Services.prefs.getBoolPref(HTTP_LOADER_PREF, false);
    if (allowed) {
      return;
    }
  } catch {
    // fall through to throw below
  }

  throw new Error(
    `Refusing to load privileged startup modules over HTTP without ${HTTP_LOADER_PREF}=true`,
  );
}

// import.meta.env.MODE
setStartupMarker("nora.startup.mode", import.meta.env.MODE);
setStartupMarker("nora.startup.loader", "");
setStartupMarker("nora.startup.test", "");
setStartupMarker("nora.startup.error", "");
if (import.meta.env.MODE === "dev") {
  assertHttpLoaderAllowed();
  try {
    //! Do not write `core/index.ts` as `core`
    //! This causes HMR error
    const loaderModule = await importWithRetry(
      "http://localhost:5181/loader/index.ts",
    );
    await loaderModule.default();
    setStartupMarker("nora.startup.loader", "loaded");
  } catch (error) {
    setStartupMarker("nora.startup.error", String(error).slice(0, 300));
    throw error;
  }
} else if (import.meta.env.MODE === "test") {
  assertHttpLoaderAllowed();
  try {
    const loaderModule = await importWithRetry(
      "http://localhost:5181/loader/index.ts",
    );
    await loaderModule.default();
    setStartupMarker("nora.startup.loader", "loaded");

    const testModule = await importWithRetry(
      "http://localhost:5181/loader/test/index.ts",
    );
    await testModule.default();
    setStartupMarker("nora.startup.test", "loaded");

    // Keep the browser alive after test completion so the host-side
    // test runner has time to collect results from prefs.js.
    // Without this the browser may shut down before the runner reads
    // the final state. Use setInterval (not an unresolved Promise) to
    // avoid blocking the event loop.
    // The interval is cleared after 10 minutes as a safety net.
    const keepaliveId = setInterval(() => {}, 60_000);
    setTimeout(() => clearInterval(keepaliveId), 600_000);
  } catch (error) {
    setStartupMarker("nora.startup.error", String(error).slice(0, 300));
    throw error;
  }
} else {
  //@ts-expect-error TS cannot find the module from chrome that is inner in Firefox
  await (await import("chrome://noraneko/content/core.js")).default();
}
