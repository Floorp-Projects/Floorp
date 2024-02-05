// This file is for testing the module loader's path handling.
// ESLint rules that modifies path shouldn't be applied.

export const obj = {};

export async function doImport() {
  // This file is loaded as resource://test/data/base_uri_module.mjs
  // Relative/absolute paths should be resolved based on the URI, instead of
  // file: path.

  const namespaceWithURI = await import(
    "resource://test/data/base_uri_module2.mjs"
  );
  const namespaceWithCurrentDir = await import("./base_uri_module2.mjs");
  const namespaceWithParentDir = await import("../data/base_uri_module2.mjs");
  const namespaceWithAbsoluteDir = await import("/data/base_uri_module2.mjs");

  return {
    equal1: namespaceWithURI.obj2 == namespaceWithCurrentDir.obj2,
    equal2: namespaceWithURI.obj2 == namespaceWithParentDir.obj2,
    equal3: namespaceWithURI.obj2 == namespaceWithAbsoluteDir.obj2,
  };
}
