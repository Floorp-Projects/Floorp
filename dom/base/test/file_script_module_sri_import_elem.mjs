import { f } from "./file_script_module_sri_import_elem_imported.mjs";

f();

window.dispatchEvent(new Event("test_evaluated"));
