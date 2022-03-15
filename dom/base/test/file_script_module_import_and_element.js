import { f } from "./file_script_module_import_and_element_imported_1.js";
import { g } from "./file_script_module_import_and_element_imported_2.js";
import { h } from "./file_script_module_import_and_element_imported_3.js";
f();
g();
h();

let script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute(
  "src",
  "file_script_module_import_and_element_imported_1.js"
);
document.body.appendChild(script);

window.dispatchEvent(new Event("test_evaluated"));
