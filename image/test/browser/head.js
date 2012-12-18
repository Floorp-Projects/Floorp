const RELATIVE_DIR = "image/test/browser/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/browser/" + RELATIVE_DIR;

var chrome_root = getRootDirectory(gTestPath);
const CHROMEROOT = chrome_root;

function getImageLoading(doc, id) {
  var htmlImg = doc.getElementById(id);
  return htmlImg.QueryInterface(Ci.nsIImageLoadingContent);
}

// Tries to get the Moz debug image, imgIContainerDebug. Only works
// in a debug build. If we succeed, we call func().
function actOnMozImage(doc, id, func) {
  var imgContainer = getImageLoading(doc, id).getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST).image;
  var mozImage;
  try {
    mozImage = imgContainer.QueryInterface(Ci.imgIContainerDebug);
  }
  catch (e) {
    return false;
  }
  func(mozImage);
  return true;
}
