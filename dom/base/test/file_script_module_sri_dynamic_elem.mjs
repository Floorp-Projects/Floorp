const { f } = await import(
  "./file_script_module_sri_dynamic_elem_imported.mjs"
);
f();

window.dispatchEvent(new Event("test_evaluated"));
