// SPDX-License-Identifier: MPL-2.0

// import.meta.env.MODE
if (import.meta.env.MODE === "dev") {
  //! Do not write `core/index.ts` as `core`
  //! This causes HMR error
  //@ts-expect-error TS cannot find the module from http
  await (await import("http://localhost:5181/loader/index.ts")).default();
} else if (import.meta.env.MODE === "test") {
  //@ts-expect-error TS cannot find the module from http
  await (await import("http://localhost:5181/loader/index.ts")).default();
  //@ts-expect-error TS cannot find the module from http
  await (await import("http://localhost:5181/loader/test/index.ts")).default();
} else {
  //@ts-expect-error TS cannot find the module from chrome that is inner in Firefox
  await (await import("chrome://noraneko/content/core.js")).default();
}
