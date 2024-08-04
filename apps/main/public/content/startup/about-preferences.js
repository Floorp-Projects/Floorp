(async () => {
  console.log((await import("chrome://noraneko/content/env.js")).envMode);
  if ((await import("chrome://noraneko/content/env.js")).envMode === "dev") {
    //! Do not write `core/index.ts` as `core`
    //! This causes HMR error
    (
      await import("http://localhost:5181/about/preferences/index.ts")
    ).default();
  } else {
    (await import("chrome://noraneko/content/about-preferences.js")).default();
  }
})();
