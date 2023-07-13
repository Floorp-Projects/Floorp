import { g } from "./file_script_module_import_multi_elems_imported_once_2.mjs";
import { h } from "./file_script_module_import_multi_elems_imported_twice.mjs";

g();
h();

window.dispatchEvent(new Event("test_evaluated"));
