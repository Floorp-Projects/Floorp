
// Toggle the prompt property
//
function setPromptOnAdd(val) {

  window.arguments[0].prompt = val;
}


// Toggle the discard duplicates property
//
function setDiscardDuplicates(val) {

  window.arguments[0].discard = val;
}


// Toggle the cancelled property
//
function setCancelled(val) {

  window.arguments[0].cancelled = val;
}


// Initialize the dialog controls
//
function loadDefaults() {

   var cb = document.getElementById("auto-discard");
   cb.setAttribute("selected", "true");
}


// Toggle the disabled attribute of the prompting checkbox
//
function togglePrompting(val) {

   var cb = document.getElementById("prompt-on-add");
   cb.setAttribute("disabled", val ? "false" : "true");

}

