function forceLoadPluginElement(id) {
  var e = document.getElementById(id);
  var found = e.pluginFoundElement;
}

function forceLoadPlugin(ids, skipRemoveAttribute) {
  if (Array.isArray(ids)) {
    ids.forEach(function(element, index, array) {
      forceLoadPluginElement(element);
    });
  } else {
    forceLoadPluginElement(ids);
  }
  if (skipRemoveAttribute) {
    return;
  }
  document.documentElement.removeAttribute("class");
}
