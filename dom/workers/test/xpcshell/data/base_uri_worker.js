// This file is for testing the module loader's path handling.
// ESLint rules that modifies path shouldn't be applied.

onmessage = async event => {
  // This file is loaded as resource://test/data/base_uri_worker.js
  // Relative/absolute paths should be resolved based on the URI, instead of
  // file: path.

  const namespaceWithURI = await import(
    "resource://test/data/base_uri_module.mjs"
  );
  const namespaceWithCurrentDir = await import("./base_uri_module.mjs");
  const namespaceWithParentDir = await import("../data/base_uri_module.mjs");
  const namespaceWithAbsoluteDir = await import("/data/base_uri_module.mjs");

  postMessage({
    scriptToModule: {
      equal1: namespaceWithURI.obj == namespaceWithCurrentDir.obj,
      equal2: namespaceWithURI.obj == namespaceWithParentDir.obj,
      equal3: namespaceWithURI.obj == namespaceWithAbsoluteDir.obj,
    },
    moduleToModuleURI: await namespaceWithURI.doImport(),
    moduleToModuleCurrent: await namespaceWithCurrentDir.doImport(),
    moduleToModuleParent: await namespaceWithParentDir.doImport(),
    moduleToModuleAbsolute: await namespaceWithAbsoluteDir.doImport(),
  });
};
