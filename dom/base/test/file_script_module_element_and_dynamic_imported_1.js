import { g } from "./file_script_module_element_and_dynamic_imported_2.js";
import { h } from "./file_script_module_element_and_dynamic_imported_3.js";
g();
h();

export function f() {}

let script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute("src", "file_script_module_element_and_dynamic.js");
document.body.appendChild(script);
