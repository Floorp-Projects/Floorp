/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
//  The first character after a period must be alphabetic.

// pref("startup.homepage_override_url","chrome://browser-region/locale/region.properties");
pref("general.startup.calendar", true);

pref("browser.chromeURL","chrome://calendar/content/");
pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindow.xul");
pref("xpinstall.dialog.confirm", "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul");
pref("xpinstall.dialog.progress", "chrome://mozapps/content/downloads/downloads.xul");
pref("xpinstall.dialog.progress.type", "Download:Manager");

// This is this application's unique identifier used by the Extension System to identify
// this application as an extension target, and by the SmartUpdate system to identify
// this application to the Update server.
pref("app.id", "{718e30fb-e89b-41dd-9da7-e25a45638b28}");
pref("app.version", 
#expand __APP_VERSION__
);
pref("app.build_id", 
#expand __BUILD_ID__
);
pref("app.extensions.version", 
#expand __APP_VERSION__
);

pref("update.app.enabled", false);
pref("update.app.url", "chrome://mozapps/locale/update/update.properties");
pref("update.app.updatesAvailable", false);
pref("update.app.updateVersion", "");
pref("update.app.updateDescription", "");
pref("update.app.updateURL", "");
pref("update.extensions.enabled", true);
pref("update.extensions.wsdl", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreExtensionsURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreThemesURL", "chrome://mozapps/locale/extensions/extensions.properties");
// Automatically download and install updates to themes and extensions.
pref("update.extensions.autoUpdate", false);

pref("update.interval", 604800000); // every 7 days
pref("update.lastUpdateDate", 0); // UTC offset when last update was performed. 

// These prefs relate to the number and severity of updates available. This is a 
// cache that the browser notification mechanism uses to determine if it should show
// status bar UI if updates are detected and the app is shut down before installing
// them.
// 0 = low (extension/theme updates), 1 = medium (app minor version), 2 = high (major version)
pref("update.severity", 0); 
// The number of extension/theme/etc updates available
pref("update.extensions.count", 0);

pref("keyword.enabled", true);
pref("keyword.URL", "http://www.google.com/search?btnI=I%27m+Feeling+Lucky&ie=UTF-8&oe=UTF-8&sourceid=mozilla-search&q=");

pref("general.useragent.locale", "chrome://global/locale/intl.properties");
pref("general.useragent.contentlocale", "chrome://browser-region/locale/region.properties");
pref("general.useragent.vendor", "Mozilla Sunbird");
pref("general.useragent.vendorSub",
#expand __APP_VERSION__
);

pref("general.smoothScroll", false);
#ifdef XP_UNIX
pref("general.autoScroll", false);
#else
pref("general.autoScroll", true);
#endif


// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
// XXXBlake Remove this stupid pref
pref("browser.startup.page",                1);
pref("browser.startup.homepage",	        "chrome://browser-region/locale/region.properties");
// "browser.startup.homepage_override" was for 4.x
pref("browser.startup.homepage_override.1", false);

pref("browser.cache.disk.capacity",         50000);
pref("browser.enable_automatic_image_resizing", true);
pref("browser.urlbar.matchOnlyTyped", false);
pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", true);
pref("browser.turbo.enabled", false);
pref("browser.formfill.enable", true);

pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 0);
pref("browser.download.manager.showAlertOnComplete", true);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", true);
pref("browser.download.manager.useWindow", true);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser-region/locale/region.properties");
// pointer to the Web Search url (content area context menu)
pref("browser.search.defaulturl", "chrome://browser-region/locale/region.properties");

// basic search popup constraint: minimum sherlock plugin version displayed
// (note: must be a string representation of a float or it'll default to 0.0)
pref("browser.search.basic.min_ver", "0.0");

pref("browser.history.grouping", "day");
pref("browser.sessionhistory.max_entries", 50);
 
// Tab browser preferences.
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.loadFolderAndReplace", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.opentabfor.urlbar", true);
pref("browser.tabs.loadBookmarksInBackground", false);

// Smart Browsing prefs
pref("browser.related.enabled", true);
pref("browser.related.autoload", 1);  // 0 = Always, 1 = After first use, 2 = Never
pref("browser.related.provider", "http://www-rl.netscape.com/wtgn?");
pref("browser.related.disabledForDomains", "");
pref("browser.goBrowsing.enabled", true);

// Default bookmark sorting
pref("browser.bookmarks.sort.direction", "descending");
pref("browser.bookmarks.sort.resource", "rdf:http://home.netscape.com/NC-rdf#Name");

// Scripts & Windows prefs
pref("dom.disable_open_during_load",        true);
pref("javascript.options.showInConsole",    true);

// popups.policy 1=allow,2=reject
pref("privacy.popups.policy",               1);
pref("privacy.popups.usecustom",            true);
pref("privacy.popups.firstTime",            true);

pref("network.protocols.useSystemDefaults", false); // set to true if user links should use system default handlers
pref("network.cookie.cookieBehavior",       0); // cookies enabled
pref("network.cookie.enableForCurrentSessionOnly", false);

// l12n and i18n
pref("intl.accept_languages", "chrome://global/locale/intl.properties");
// collationOption is only set on linux for japanese. see bug 18338 and 62015
// we need to check if this pref is still useful.
pref("intl.collationOption",  "chrome://global-platform/locale/intl.properties");
pref("intl.charsetmenu.browser.static", "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more1",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more2",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more3",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more4",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more5",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.unicode",  "chrome://global/locale/intl.properties");
pref("intl.charset.detector", "chrome://global/locale/intl.properties");
pref("intl.charset.default",  "chrome://global-platform/locale/intl.properties");
pref("font.language.group", "chrome://global/locale/intl.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");

// 0=lines, 1=pages, 2=history , 3=text size
pref("mousewheel.withcontrolkey.action",3);
pref("mousewheel.withshiftkey.action",2);
pref("mousewheel.withaltkey.action",0);

pref("profile.allow_automigration", false);   // setting to false bypasses automigration in the profile code

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");
pref("browser.throbber.url","chrome://browser-region/locale/region.properties");

// pref to control the alert notification 
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 4000);
pref("alerts.height", 50);

// update notifications prefs
pref("update_notifications.enabled", true);
pref("update_notifications.provider.0.frequency", 7); // number of days
pref("update_notifications.provider.0.datasource", "chrome://browser-region/locale/region.properties");

pref("browser.xul.error_pages.enabled", false);

pref("signon.rememberSignons",              true);
pref("signon.expireMasterPassword",         false);
pref("signon.SignonFileName", "signons.txt");

pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news" , true); // for news 

// By default, all protocol handlers are exposed.  This means that
// the browser will respond to openURL commands for all URL types.
// It will also try to open link clicks inside the browser before
// failing over to the system handlers.
pref("network.protocol-handler.expose-all", true);
pref("network.protocol-handler.expose.mailto", false);

// Default security warning dialogs to show once.
pref("security.warn_entering_secure.show_once", true);
pref("security.warn_entering_weak.show_once", true);
pref("security.warn_leaving_secure.show_once", true);
pref("security.warn_viewing_mixed.show_once", true);
pref("security.warn_submit_insecure.show_once", true);

pref("browser.urlbar.clickSelectsAll", true);
#ifdef XP_UNIX
#ifndef XP_MACOSX
pref("browser.urlbar.clickSelectsAll", false);
#endif
#endif

