const { f } = await import(
  "./file_script_module_dynamic_and_static_imported_1.mjs"
);
import { g } from "./file_script_module_dynamic_and_static_imported_2.mjs";
import { h } from "./file_script_module_dynamic_and_static_imported_3.mjs";

f();
g();
h();

window.dispatchEvent(new Event("test_evaluated"));
