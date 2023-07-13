import { f } from "./file_script_module_sri_import_elem_nopreload_imported.mjs";

f();

// Dynamically insert the script element in order to suppress preload.
const script = document.createElement("script");
script.id = "watchme2";
script.setAttribute("type", "module");
script.setAttribute(
  "src",
  "file_script_module_sri_import_elem_nopreload_imported.mjs"
);
script.setAttribute(
  "integrity",
  "sha384-3XSIfAj4/GALfWzL3T89+t3eaLIY59g8IWz1qq59xKnEW3aGd4cz7XvdcYqoK2+J"
);
document.body.appendChild(script);

window.dispatchEvent(new Event("test_evaluated"));
