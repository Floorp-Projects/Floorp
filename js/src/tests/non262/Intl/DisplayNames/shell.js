// Add |Intl.MozDisplayNames| to the Intl object.
function addMozIntlDisplayNames(global) {
  let obj = {};
  global.addIntlExtras(obj);
  global.Intl.DisplayNames = obj.DisplayNames;
}
