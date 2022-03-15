function relay(event) {
  if (event.type != "test_evaluated") {
    if (!/^watchme/.test(event.target.id)) {
      return;
    }
  }

  const type = `${window.name}_${event.type}`;

  window.parent.dispatchEvent(new window.parent.Event(type));
}

window.addEventListener("scriptloader_load_source", relay);
window.addEventListener("scriptloader_load_bytecode", relay);
window.addEventListener("scriptloader_execute", relay);
window.addEventListener("scriptloader_evaluate_module", relay);
window.addEventListener("scriptloader_encode", relay);
window.addEventListener("scriptloader_no_encode", relay);
window.addEventListener("scriptloader_bytecode_saved", relay);
window.addEventListener("scriptloader_bytecode_failed", relay);
window.addEventListener("scriptloader_fallback", relay);
window.addEventListener("test_evaluated", relay);
