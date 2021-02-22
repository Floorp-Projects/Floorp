// |reftest| shell-option(--enable-top-level-await) module async

// Load and instantiate "bug1488117-import-namespace.js". "bug1488117-import-namespace.js"
// contains an |import*| request for the current module, which triggers GetModuleNamespace for
// this module. GetModuleNamespace calls GetExportedNames on the current module, which in turn
// resolves and calls GetExportedNames on all |export*| entries.
// And that means HostResolveImportedModule is called for "bug1488117-empty.js" before
// InnerModuleInstantiation for "bug1488117.js" has resolved that module file.

async function test() {
  try {
    await import("./bug1689499-a.js");
    throw new Error("import should have failed");
  } catch (exc) {
    assertEq(exc.message, "FAIL");
  }

  try {
    await import("./bug1689499-x.js");
    throw new Error("import should have failed");
  } catch (exc) {
    assertEq(exc.message, "FAIL");
  }

  if (typeof reportCompare === "function")
        reportCompare(0, 0);
}

test();
