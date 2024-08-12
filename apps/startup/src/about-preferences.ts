//console.log((await import("chrome://noraneko/content/env.js")).envMode);
if (import.meta.env.MODE === "dev") {
  //! Do not write `core/index.ts` as `core`
  //! This causes HMR error
  (await import("http://localhost:5181/about/preferences/index.ts")).default();
} else if (import.meta.env.MODE === "test") {
  (await import("http://localhost:5181/about/preferences/index.ts")).default();
  (
    await import("http://localhost:5181/about/preferences/index.test.ts")
  ).default();
} else {
  (await import("chrome://noraneko/content/about-preferences.js")).default();
}
export type {};
