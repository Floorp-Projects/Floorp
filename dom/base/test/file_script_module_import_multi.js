import { f } from "./file_script_module_import_multi_imported_once.js";
import { g } from "./file_script_module_import_multi_imported_twice.js";
f();
g();

window.dispatchEvent(new Event("test_evaluated"));
