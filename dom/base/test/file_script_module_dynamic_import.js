const { f } = await import("./file_script_module_dynamic_import_imported.js");
f();

window.dispatchEvent(new Event("test_evaluated"));
