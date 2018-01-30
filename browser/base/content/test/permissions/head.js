Cu.import("resource:///modules/SitePermissions.jsm", this);

function is_hidden(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding", "closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

