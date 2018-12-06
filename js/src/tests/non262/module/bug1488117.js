// |reftest| module

// Load and instantiate "bug1488117-import-namespace.js". "bug1488117-import-namespace.js"
// contains an |import*| request for the current module, which triggers GetModuleNamespace for
// this module. GetModuleNamespace calls GetExportedNames on the current module, which in turn
// resolves and calls GetExportedNames on all |export*| entries.
// And that means HostResolveImportedModule is called for "bug1488117-empty.js" before
// InnerModuleInstantiation for "bug1488117.js" has resolved that module file.

import "./bug1488117-import-namespace.js";
export* from "./bug1488117-empty.js";

if (typeof reportCompare === "function")
    reportCompare(0, 0);
