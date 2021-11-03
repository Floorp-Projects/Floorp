===========================
Access debugging in add-ons
===========================

.. warning::
  We are planning to deprecate the use by Firefox add-ons of the techniques described in this document. Don't write new add-ons that use these techniques.

The following items are accessible in the context of chrome://browser/content/debugger.xul (or, in version 23 beta, chrome://browser/content/devtools/debugger.xul):


- window.addEventListener("Debugger:EditorLoaded") - called when the read-only script panel loaded.
- window.addEventListener("Debugger:EditorUnloaded")


Relevant files:


- chrome://browser/content/devtools/debugger-controller.js
- chrome://browser/content/devtools/debugger-toolbar.js
- chrome://browser/content/devtools/debugger-view.js
- chrome://browser/content/devtools/debugger-panes.js


Unfortunately there is not yet any API to evaluate watches/expressions within the debugged scope, or highlight elements on the page that are referenced as variables in the debugged scope. (currently a work in progress, see bug `653545 <https://bugzilla.mozilla.org/show_bug.cgi?id=653545>`_.)
