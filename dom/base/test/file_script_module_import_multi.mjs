import { f } from "./file_script_module_import_multi_imported_once.mjs";
import { g } from "./file_script_module_import_multi_imported_twice.mjs";

f();
g();

window.dispatchEvent(new Event("test_evaluated"));
