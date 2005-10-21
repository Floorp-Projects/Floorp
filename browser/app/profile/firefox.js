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

// XXX Toolkit-specific preferences should be moved into toolkit.js

#filter substitution

# SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
#  The first character after a period must be alphabetic.

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define UNIX_BUT_NOT_MAC
#endif
#endif

pref("startup.homepage_override_url","chrome://browser-region/locale/region.properties");
pref("general.startup.browser", true);

pref("browser.chromeURL","chrome://browser/content/");
pref("browser.hiddenWindowChromeURL", "chrome://browser/content/hiddenWindow.xul");
pref("xpinstall.dialog.confirm", "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul");
pref("xpinstall.dialog.progress.skin", "chrome://mozapps/content/extensions/extensions.xul?type=themes");
pref("xpinstall.dialog.progress.chrome", "chrome://mozapps/content/extensions/extensions.xul?type=extensions");
pref("xpinstall.dialog.progress.type.skin", "Extension:Manager-themes");
pref("xpinstall.dialog.progress.type.chrome", "Extension:Manager-extensions");

pref("extensions.getMoreExtensionsURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreThemesURL", "chrome://mozapps/locale/extensions/extensions.properties");
// Developers can set this to |true| if they are constantly changing files in their 
// extensions directory so that the extension system does not constantly think that
// their extensions are being updated and thus reregistered every time the app is
// started.
pref("extensions.ignoreMTimeChanges", false);
// Enables some extra Extension System Logging (can reduce performance)
pref("extensions.logging.enabled", false);

// App-specific update preferences

// Whether or not app updates are enabled
pref("app.update.enabled", true);

// This preference turns on app.update.mode and allows automatic download and
// install to take place. We use a separate boolean toggle for this to make
// the UI easier to construct.
pref("app.update.auto", true);

// Defines how the Application Update Service notifies the user about updates:
//
// AUM Set to:        Minor Releases:     Major Releases:
// 0                  download no prompt  download no prompt
// 1                  download no prompt  download no prompt if no incompatibilities
// 2                  download no prompt  prompt
//
// See chart in nsUpdateService.js.in for more details
//
pref("app.update.mode", 1);

// If set to true, the Update Service will present no UI for any event.
pref("app.update.silent", false);

// Update service URL:
pref("app.update.url", "https://aus2.mozilla.org/update/1/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/update.xml");
// URL user can browse to manually if for some reason all update installation
// attempts fail.  TODO: Change this URL
pref("app.update.url.manual", "http://www.mozilla.org/products/firefox/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "chrome://browser-region/locale/region.properties");

// User-settable override to app.update.url for testing purposes.
//pref("app.update.url.override", "");

// Interval: Time between checks for a new version (in seconds)
//           default=1 day
pref("app.update.interval", 86400);
// Interval: Time before prompting the user to download a new version that 
//           is available (in seconds) default=1 day
pref("app.update.nagTimer.download", 86400);
// Interval: Time before prompting the user to restart to install the latest
//           download (in seconds) default=30 minutes
pref("app.update.nagTimer.restart", 1800);
// Interval: When all registered timers should be checked (in milliseconds)
//           default=5 seconds
pref("app.update.timer", 600000);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied. This is off in Firefox by default since we show a 
// upgrade start page instead! Other apps may wish to show this UI, and supply
// a whatsNewURL field in their brand.properties that contains a link to a page
// which tells users what's new in this new update.
pref("app.update.showInstalledUI", false);

// 0 = suppress prompting for incompatibilities if there are updates available
//     to newer versions of installed addons that resolve them.
// 1 = suppress prompting for incompatibilities only if there are VersionInfo
//     updates available to installed addons that resolve them, not newer
//     versions.
pref("app.update.incompatible.mode", 0);

// Symmetric (can be overridden by individual extensions) update preferences.
// e.g.
//  extensions.{GUID}.update.enabled
//  extensions.{GUID}.update.url
//  extensions.{GUID}.update.interval
//  .. etc ..
//
pref("extensions.update.enabled", true);
pref("extensions.update.url", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.update.interval", 86400);  // Check for updates to Extensions and 
                                            // Themes every week
// Non-symmetric (not shared by extensions) extension-specific [update] preferences
pref("extensions.getMoreExtensionsURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreThemesURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.dss.enabled", false);          // Dynamic Skin Switching                                               
pref("extensions.dss.switchPending", false);    // Non-dynamic switch pending after next
                                                // restart.

pref("xpinstall.whitelist.add", "update.mozilla.org");
pref("xpinstall.whitelist.add.103", "addons.mozilla.org");

pref("keyword.enabled", true);
pref("keyword.URL", "http://www.google.com/search?btnI=I%27m+Feeling+Lucky&ie=UTF-8&oe=UTF-8&q=");

pref("general.useragent.locale", "@AB_CD@");
pref("general.skins.selectedSkin", "classic/1.0");
pref("general.useragent.extra.firefox", "Firefox/@APP_VERSION@");

pref("general.smoothScroll", false);
#ifdef UNIX_BUT_NOT_MAC
pref("general.autoScroll", false);
#else
pref("general.autoScroll", true);
#endif

// Whether or not the application should check at startup each time if it 
// is the default browser.
pref("browser.shell.checkDefaultBrowser", true);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
// XXXBlake Remove this stupid pref
pref("browser.startup.page",                1);
pref("browser.startup.homepage",	          "resource:/browserconfig.properties");

pref("browser.cache.disk.capacity",         50000);
pref("browser.enable_automatic_image_resizing", true);
pref("browser.urlbar.matchOnlyTyped", false);
pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", true);
pref("browser.formfill.enable", true);

pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 0);
#ifdef XP_WIN
pref("browser.download.manager.showAlertOnComplete", true);
#else
pref("browser.download.manager.showAlertOnComplete", false);
#endif
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", true);
pref("browser.download.manager.useWindow", true);
pref("browser.download.manager.closeWhenDone", false);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);

// pointer to the default engine name
pref("browser.search.defaultenginename",      "chrome://browser-region/locale/region.properties");
// pointer to the Web Search url (content area context menu)
pref("browser.search.defaulturl",             "chrome://browser-region/locale/region.properties");
// Ordering of Search Engines in the Engine list. 
pref("browser.search.order.1",                "chrome://browser-region/locale/region.properties");
pref("browser.search.order.2",                "chrome://browser-region/locale/region.properties");
 
pref("browser.search.param.Google.1.default", "chrome://branding/content/searchconfig.properties");
pref("browser.search.param.Google.1.custom",  "chrome://branding/content/searchconfig.properties");
pref("browser.search.order.Yahoo.1",          "chrome://branding/content/searchconfig.properties");
pref("browser.search.order.Yahoo.2",          "chrome://branding/content/searchconfig.properties");
pref("browser.search.order.Yahoo",            "chrome://branding/content/searchconfig.properties");

// basic search popup constraint: minimum sherlock plugin version displayed
// (note: must be a string representation of a float or it'll default to 0.0)
pref("browser.search.basic.min_ver", "0.0");

// search bar results always open in a new tab
pref("browser.search.openintab", false);

// send ping to the server to update
pref("browser.search.update", true);

pref("browser.history.grouping", "day");
pref("browser.sessionhistory.max_entries", 50);

// handle external links
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most recent window
pref("browser.link.open_external", 3);

// handle links targeting new windows
pref("browser.link.open_newwindow", 2);

// 0: no restrictions - divert everything
// 1: don't divert window.open at all
// 2: don't divert window.open with features
pref("browser.link.open_newwindow.restriction", 2);

// Tab browser preferences.
pref("browser.tabs.loadInBackground", true);
pref("browser.tabs.loadFolderAndReplace", true);
pref("browser.tabs.opentabfor.middleclick", true);
pref("browser.tabs.loadDivertedInBackground", false);
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
pref("dom.disable_open_during_load",              true);
pref("javascript.options.showInConsole",          false);
// Make the status bar reliably present and unaffected by pages
pref("dom.disable_window_open_feature.status",    true);
// This is the pref to control the location bar, change this to true to 
// force this instead of or in addition to the status bar - this makes 
// the origin of popup windows more obvious to avoid spoofing but we 
// cannot do it by default because it affects UE for web applications.
pref("dom.disable_window_open_feature.location",  false);
pref("dom.disable_window_status_change",          true);
// prevent JS from moving/resizing existing windows
pref("dom.disable_window_move_resize",            true);
// prevent JS from monkeying with window focus, etc
pref("dom.disable_window_flip",                   true);
 
pref("browser.trim_user_and_password",            true);

// popups.policy 1=allow,2=reject
pref("privacy.popups.policy",               1);
pref("privacy.popups.usecustom",            true);
pref("privacy.popups.firstTime",            true);
pref("privacy.popups.showBrowserMessage",   true);
 
pref("privacy.item.history",    true);
pref("privacy.item.formdata",   true);
pref("privacy.item.passwords",  false);
pref("privacy.item.downloads",  true);
pref("privacy.item.cookies",    false);
pref("privacy.item.cache",      true);
pref("privacy.item.siteprefs",  false);
pref("privacy.item.sessions",   true);

pref("privacy.sanitize.sanitizeOnShutdown", false);
pref("privacy.sanitize.promptOnSanitize", true);

pref("network.proxy.share_proxy_settings",  false); // use the same proxy settings for all protocols

pref("network.cookie.cookieBehavior",       0); // cookies enabled
pref("network.cookie.enableForCurrentSessionOnly", false);
pref("network.cookie.denyRemovedCookies", false);

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
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");

// 0=lines, 1=pages, 2=history , 3=text size
#ifdef XP_MACOSX
// On OS X, if the wheel has one axis only, shift+wheel comes through as a
// horizontal scroll event. Thus, we can't assign anything other than normal
// scrolling to shift+wheel.
pref("mousewheel.withmetakey.action",3);
pref("mousewheel.withmetakey.sysnumlines",false);
pref("mousewheel.withcontrolkey.action",2);
pref("mousewheel.withcontrolkey.sysnumlines",false);
#else
pref("mousewheel.withcontrolkey.action",3);
pref("mousewheel.withcontrolkey.sysnumlines",false);
pref("mousewheel.withshiftkey.action",2);
pref("mousewheel.withshiftkey.sysnumlines",false);
#endif
pref("mousewheel.withaltkey.action",0);
pref("mousewheel.withaltkey.sysnumlines",false);

pref("profile.allow_automigration", false);   // setting to false bypasses automigration in the profile code

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");
pref("browser.throbber.url","chrome://browser-region/locale/region.properties");

// pref to control the alert notification 
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 4000);
pref("alerts.height", 50);

pref("browser.xul.error_pages.enabled", true);

pref("signon.rememberSignons",              true);
pref("signon.expireMasterPassword",         false);
pref("signon.SignonFileName", "signons.txt");

// We want to make sure mail URLs are handled externally...
pref("network.protocol-handler.external.mailto", true); // for mail
pref("network.protocol-handler.external.news", true);   // for news
pref("network.protocol-handler.external.snews", true);  // for secure news
pref("network.protocol-handler.external.nntp", true);   // also news
// ...without warning dialogs
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.nntp", false);

// By default, all protocol handlers are exposed.  This means that
// the browser will respond to openURL commands for all URL types.
// It will also try to open link clicks inside the browser before
// failing over to the system handlers.
pref("network.protocol-handler.expose-all", true);
pref("network.protocol-handler.expose.mailto", false);
pref("network.protocol-handler.expose.news", false);
pref("network.protocol-handler.expose.snews", false);
pref("network.protocol-handler.expose.nntp", false);

// Default security warning dialogs to show once.
pref("security.warn_entering_secure.show_once", true);
pref("security.warn_entering_weak.show_once", true);
pref("security.warn_leaving_secure.show_once", true);
pref("security.warn_viewing_mixed.show_once", true);
pref("security.warn_submit_insecure.show_once", true);

#ifdef XP_UNIX
pref("browser.urlbar.clickSelectsAll", false);
#else
pref("browser.urlbar.clickSelectsAll", true);
#endif

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.flashBar", 1);

// Disable the default plugin for firefox
pref("plugin.default_plugin_disabled", true);

// plugin finder service url
pref("pfs.datasource.url", "https://pfs.mozilla.org/plugins/PluginFinderService.php?mimetype=%PLUGIN_MIMETYPE%&appID=%APP_ID%&appVersion=%APP_VERSION%&clientOS=%CLIENT_OS%&chromeLocale=%CHROME_LOCALE%");

#ifdef XP_WIN
pref("browser.preferences.instantApply", false);
#else
pref("browser.preferences.instantApply", true);
#endif
#ifdef XP_MACOSX
pref("browser.preferences.animateFadeIn", true);
#else
pref("browser.preferences.animateFadeIn", false);
#endif
#ifndef XP_OS2
pref("browser.display.screen_resolution", 96);
#endif

pref("browser.download.show_plugins_in_list", true);
pref("browser.download.hide_plugins_without_extensions", true);

// Setting this pref to |true| forces BiDi UI menu items and keyboard shortcuts
// to be exposed. By default, only expose it for bidi-associated system locales.
pref("bidi.browser.ui", false);

// Backspace and Shift+Backspace behavior
// 0 goes Back/Forward
// 1 act like PgUp/PgDown
// 2 and other values, nothing
pref("browser.backspace_action", 0);
