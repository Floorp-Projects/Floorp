if (import.meta.env.MODE === "dev") {
  //! Do not write `core/index.ts` as `core`
  //! This causes HMR error
  await (await import("http://localhost:5181/core/index.ts")).default();
} else if (import.meta.env.MODE === "test") {
  await (await import("http://localhost:5181/core/index.ts")).default();
  await (await import("http://localhost:5181/core/test/index.ts")).default();
} else {
  await (await import("chrome://noraneko/content/core.js")).default();
}

export type {};
