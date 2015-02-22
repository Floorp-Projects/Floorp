function forceLoadPluginElement(id) {
  var e = document.getElementById(id);
  var found = e.pluginFoundElement;
}

function forceLoadPlugin(ids, dontRemoveClassAttribute) {
  if (Array.isArray(ids)) {
    ids.forEach(function(element, index, array) {
      forceLoadPluginElement(element);
    });
  } else {
    forceLoadPluginElement(ids);
  }
  if (dontRemoveClassAttribute) {
    // In some tests we need to do other stuff instead of removing the class
    // attribute. In that case we pass a function and run that before returning.
    if (typeof dontRemoveClassAttribute === 'function') {
      dontRemoveClassAttribute();
    }
    return;
  }
  document.documentElement.removeAttribute("class");
}
