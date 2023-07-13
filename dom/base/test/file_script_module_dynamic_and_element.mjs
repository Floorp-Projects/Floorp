const { f } = await import(
  "./file_script_module_dynamic_and_element_imported_1.mjs"
);
import { g } from "./file_script_module_dynamic_and_element_imported_2.mjs";
import { h } from "./file_script_module_dynamic_and_element_imported_3.mjs";

f();
g();
h();

let script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute(
  "src",
  "file_script_module_dynamic_and_element_imported_1.mjs"
);
document.body.appendChild(script);

window.dispatchEvent(new Event("test_evaluated"));
