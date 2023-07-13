import { f } from "./file_script_module_static_and_dynamic_imported_1.mjs";
import { g } from "./file_script_module_static_and_dynamic_imported_2.mjs";
import { h } from "./file_script_module_static_and_dynamic_imported_3.mjs";

f();
g();
h();

window.dispatchEvent(new Event("test_evaluated"));
