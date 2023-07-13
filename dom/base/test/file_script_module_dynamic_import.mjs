const { f } = await import("./file_script_module_dynamic_import_imported.mjs");
f();

window.dispatchEvent(new Event("test_evaluated"));
