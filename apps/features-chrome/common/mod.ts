export function getFeaturesCommonEntries() {
  return import.meta.glob("./*/index.ts")
}