import { f } from "./file_script_module_import_imported.mjs";

f();

window.dispatchEvent(new Event("test_evaluated"));
