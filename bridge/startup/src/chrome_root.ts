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
    // @ts-expect-error Services is provided in browser chrome context.
    Services.prefs.setStringPref(key, value);
  } catch {
    // Diagnostics should never block startup flow.
  }
}

function assertHttpLoaderAllowed(): void {
  try {
    // @ts-expect-error Services is provided in browser chrome context.
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
    // @ts-expect-error TS cannot find the module from http
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
    // @ts-expect-error TS cannot find the module from http
    await loaderModule.default();
    setStartupMarker("nora.startup.loader", "loaded");

    const testModule = await importWithRetry(
      "http://localhost:5181/loader/test/index.ts",
    );
    // @ts-expect-error TS cannot find the module from http
    await testModule.default();
    setStartupMarker("nora.startup.test", "loaded");
  } catch (error) {
    setStartupMarker("nora.startup.error", String(error).slice(0, 300));
    throw error;
  }
} else {
  //@ts-expect-error TS cannot find the module from chrome that is inner in Firefox
  await (await import("chrome://noraneko/content/core.js")).default();
}
