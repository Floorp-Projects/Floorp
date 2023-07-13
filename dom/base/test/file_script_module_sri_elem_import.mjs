import { f } from "./file_script_module_sri_elem_import_imported.mjs";

f();

window.dispatchEvent(new Event("test_evaluated"));
