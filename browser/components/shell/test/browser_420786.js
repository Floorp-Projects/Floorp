const DG_BACKGROUND = "/desktop/gnome/background"
const DG_IMAGE_KEY = DG_BACKGROUND + "/picture_filename";
const DG_OPTION_KEY = DG_BACKGROUND + "/picture_options";
const DG_DRAW_BG_KEY = DG_BACKGROUND + "/draw_background";

function onPageLoad() {
  gBrowser.selectedBrowser.removeEventListener("load", onPageLoad, true);

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

  var image = content.document.images[0];

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
  checkWallpaper(Ci.nsIShellService.BACKGROUND_FILL, "zoom");
  checkWallpaper(Ci.nsIShellService.BACKGROUND_FIT, "scaled");

  // Restore GConf and wallpaper

  gconf.setString(DG_IMAGE_KEY, prevImageKey);
  gconf.setString(DG_OPTION_KEY, prevOptionKey);
  gconf.setBool(DG_DRAW_BG_KEY, prevDrawBgKey);

  wpFile.remove(false);
  if (wpFileBackup.exists())
    wpFileBackup.moveTo(null, wpFile.leafName);

  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  var osString = Cc["@mozilla.org/xre/app-info;1"].
                 getService(Ci.nsIXULRuntime).OS;
  if (osString != "Linux") {
    todo(false, "This test is Linux specific for now.");
    return;
  }

  try {
    // If GSettings is available, then the GConf tests
    // will fail
    var gsettings = Cc["@mozilla.org/gsettings-service;1"].
                    getService(Ci.nsIGSettingsService).
                    getCollectionForSchema("org.gnome.desktop.background");
    todo(false, "This test doesn't work when GSettings is available");
    return;
  } catch(e) { }

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", onPageLoad, true);
  content.location = "about:logo";

  waitForExplicitFinish();
}
