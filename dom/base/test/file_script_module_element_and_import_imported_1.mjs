import { g } from "./file_script_module_element_and_import_imported_2.mjs";
import { h } from "./file_script_module_element_and_import_imported_3.mjs";

g();
h();

export function f() {}

let script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute("src", "file_script_module_element_and_import.mjs");
document.body.appendChild(script);
