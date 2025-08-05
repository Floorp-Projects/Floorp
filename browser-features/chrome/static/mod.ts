export function getFeaturesStaticEntries() {
  return import.meta.glob("./*/index.ts");
}
