function scaleRatio(scale) {
  if (navigator.appVersion.indexOf("Android") >= 0) {
    return {"set": [["browser.viewport.scaleRatio", 100*scale]]};
  }
  return {"set": [["layout.css.devPixelsPerPx", "" + scale]]};
}
