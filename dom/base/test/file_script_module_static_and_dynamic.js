import { f } from "./file_script_module_static_and_dynamic_imported_1.js";
import { g } from "./file_script_module_static_and_dynamic_imported_2.js";
import { h } from "./file_script_module_static_and_dynamic_imported_3.js";
f();
g();
h();

window.dispatchEvent(new Event("test_evaluated"));
