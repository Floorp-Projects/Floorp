import { f } from "./file_script_module_import_multi_elems_imported_once_1.mjs";
import { h } from "./file_script_module_import_multi_elems_imported_twice.mjs";

f();
h();

// Dynamically insert the element after loading all source, so that
// the module import doesn't race.
const script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute("src", "file_script_module_import_multi_elems_2.mjs");
document.body.appendChild(script);

window.dispatchEvent(new Event("test_evaluated"));
