const Ci = Components.interfaces;
const Cc = Components.classes;

const DG_BACKGROUND = "/desktop/gnome/background"
const DG_IMAGE_KEY = DG_BACKGROUND + "/picture_filename";
const DG_OPTION_KEY = DG_BACKGROUND + "/picture_options";
const DG_DRAW_BG_KEY = DG_BACKGROUND + "/draw_background";

var testPage;

function url(spec) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(spec, null, null);
}

function onPageLoad() {
  testPage.events.removeListener("load", onPageLoad);

  var bs = Cc["@mozilla.org/intl/stringbundle;1"].
           getService(Ci.nsIStringBundleService);
  var brandName = bs.createBundle("chrome://branding/locale/brand.properties").
                  GetStringFromName("brandShortName");

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIDirectoryServiceProvider);
  var homeDir = dirSvc.getFile("Home", {});

  var wpFile = homeDir.clone();
  wpFile.append(brandName + "_wallpaper.png");

  // Backup the existing wallpaper so that this test doesn't change the user's
  // settings.
  var wpFileBackup = homeDir.clone()
  wpFileBackup.append(brandName + "_wallpaper.png.backup");

  if (wpFileBackup.exists())
    wpFileBackup.remove(false);

  if (wpFile.exists())
    wpFile.copyTo(null, wpFileBackup.leafName);

  var shell = Cc["@mozilla.org/browser/shell-service;1"].
              getService(Ci.nsIShellService);
  var gconf = Cc["@mozilla.org/gnome-gconf-service;1"].
              getService(Ci.nsIGConfService);

  var prevImageKey = gconf.getString(DG_IMAGE_KEY);
  var prevOptionKey = gconf.getString(DG_OPTION_KEY);
  var prevDrawBgKey = gconf.getBool(DG_DRAW_BG_KEY);

  var image = testPage.document.getElementsByTagName("img")[0];

  function checkWallpaper(position, expectedGConfPosition) {
    shell.setDesktopBackground(image, position);
    ok(wpFile.exists(), "Wallpaper was written to disk");
    is(gconf.getString(DG_IMAGE_KEY), wpFile.path,
       "Wallpaper file GConf key is correct");
    is(gconf.getString(DG_OPTION_KEY), expectedGConfPosition,
       "Wallpaper position GConf key is correct");
  }

  checkWallpaper(Ci.nsIShellService.BACKGROUND_TILE, "wallpaper");
  checkWallpaper(Ci.nsIShellService.BACKGROUND_STRETCH, "stretched");
  checkWallpaper(Ci.nsIShellService.BACKGROUND_CENTER, "centered");
  checkWallpaper(Ci.nsIShellService.BACKGROUND_FILL, "centered");

  // Restore GConf and wallpaper

  gconf.setString(DG_IMAGE_KEY, prevImageKey);
  gconf.setString(DG_OPTION_KEY, prevOptionKey);
  gconf.setBool(DG_DRAW_BG_KEY, prevDrawBgKey);

  wpFile.remove(false);
  if (wpFileBackup.exists()) {
   wpFileBackup.moveTo(null, wpFile.leafName);
  }
  testPage.close();
  finish();
}

function test() {
  var osString = Cc["@mozilla.org/xre/app-info;1"].
                 getService(Ci.nsIXULRuntime).OS;

  // This test is Linux specific for now
  if (osString != "Linux") {
    finish();
    return;
  }

  testPage = Application.activeWindow.open(url("about:blank"));
  testPage.events.addListener("load", onPageLoad);
  testPage.load(url("about:logo"));
  testPage.focus();

  waitForExplicitFinish();
}
