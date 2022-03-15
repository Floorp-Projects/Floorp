import { f } from "./file_script_module_import_multi_elems_imported_once_1.js";
import { h } from "./file_script_module_import_multi_elems_imported_twice.js";
f();
h();

window.dispatchEvent(new Event("test_evaluated"));
