if (import.meta.env.MODE === "dev") {
  //! Do not write `core/index.ts` as `core`
  //! This causes HMR error
  (await import("http://localhost:5181/core/index.ts")).default();
} else if (import.meta.env.MODE === "test") {
  (await import("http://localhost:5181/core/index.ts")).default();
  (await import("http://localhost:5181/core/test/index.ts")).default();
} else {
  (await import("chrome://noraneko/content/core.js")).default();
}

export type {};
