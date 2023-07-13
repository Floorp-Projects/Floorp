import { f } from "./file_script_module_element_and_import_imported_1.mjs";
import { g } from "./file_script_module_element_and_import_imported_2.mjs";
import { h } from "./file_script_module_element_and_import_imported_3.mjs";

f();
g();
h();

window.dispatchEvent(new Event("test_evaluated"));
