/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

pref("general.useragent.vendor", "Thunderbird");
pref("general.useragent.vendorSub",
#expand __APP_VERSION__
);

// This is this application's unique identifier used by the Extension System to identify
// this application as an extension target, and by the SmartUpdate system to identify
// this application to the Update server.
pref("app.id", "{3550f703-e582-4d05-9a08-453d09bdfdc6}");
pref("app.version", 
#expand __APP_VERSION__
);
pref("app.build_id", 
#expand __BUILD_ID__
);
pref("app.extensions.version", "0.6");

#ifdef XP_MACOSX
pref("mail.biff.animate_dock_icon", false);
#endif

pref("update.app.enabled", true);
pref("update.app.url", "chrome://mozapps/locale/update/update.properties");
pref("update.app.updatesAvailable", false);
pref("update.app.updateVersion", "");
pref("update.app.updateDescription", "");
pref("update.app.updateURL", "");
pref("update.extensions.enabled", true);
pref("update.extensions.wsdl", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreExtensionsURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreThemesURL", "chrome://mozapps/locale/extensions/extensions.properties");

pref("update.extensions.autoUpdate", false);    // Automatically download and install 
                                                // updates to themes and extensions. 
                                                // Does nothing at present. 
pref("update.extensions.interval", 604800000);  // Check for updates to Extensions and 
                                                // Themes every week
pref("update.extensions.lastUpdateDate", 0);    // UTC offset when last Extension/Theme 
                                                // update was performed. 
pref("update.extensions.severity.threshold", 5);// The number of pending Extension/Theme
                                                // updates you can have before the update
                                                // notifier goes from low->medium severity.
pref("update.app.interval", 86400000);          // Check for updates to Firefox every day
pref("update.app.lastUpdateDate", 0);           // UTC offset when last App update was 
                                                // performed. 
pref("update.interval", 3600000);               // Check each of the above intervals 
                                                // every 60 mins
pref("update.showSlidingNotification", false);   // Windows-only slide-up taskbar 
                                                // notification.

// These prefs relate to the number and severity of updates available. This is a 
// cache that the browser notification mechanism uses to determine if it should show
// status bar UI if updates are detected and the app is shut down before installing
// them.
// 0 = low    (extension/theme updates), 
// 1 = medium (numerous extension/theme updates), 
// 2 = high   (new version of Firefox/Security patch)
pref("update.severity", 0); 
// The number of extension/theme/etc updates available
pref("update.extensions.count", 0);
pref("xpinstall.whitelist.add", "update.mozilla.org");


/////////////////////////////////////////////////////////////////
// Overrides of the seamonkey suite mailnews.js prefs
///////////////////////////////////////////////////////////////// 
pref("mail.showFolderPaneColumns", false); // setting to true will allow total/unread/size columns
pref("mail.showCondensedAddresses", true); // show the friendly display name for people I know

pref("mailnews.message_display.allow.plugins", false); // disable plugins by default
pref("mailnews.message_display.disable_remote_image", true);
pref("mailnews.message_display.disable_remote_images.useWhitelist", true);
pref("mailnews.message_display.disable_remote_images.whiteListAbURI","moz-abmdbdirectory://abook.mab");

// hidden pref for changing how we present attachments in the message pane
pref("mailnews.attachments.display.largeView", false); 
pref("mail.pane_config.dynamic",            0);
pref("mailnews.display.sanitizeJunkMail", true);
pref("mail.standalone", true); 
pref("editor.singleLine.pasteNewlines", 4);  // substitute commas for new lines in single line text boxes

// hidden pref to ensure a certain number of headers in the message pane
// to avoid the height of the header area from changing when headers are present / not present
pref("mailnews.headers.minNumHeaders", 0); // 0 means we ignore this pref

pref("mail.compose.dontWarnMail2Newsgroup", false);

pref("messenger.throbber.url","chrome://messenger-region/locale/region.properties");
pref("mailnews.release_notes.url","chrome://messenger-region/locale/region.properties");
pref("mailnews.hints_and_tips.url","chrome://messenger-region/locale/region.properties");
pref("compose.throbber.url","chrome://messenger-region/locale/region.properties");
pref("addressbook.throbber.url","chrome://messenger-region/locale/region.properties");

pref("network.image.imageBehavior", 2);

/////////////////////////////////////////////////////////////////
// End seamonkey suite mailnews.js pref overrides
///////////////////////////////////////////////////////////////// 

#ifdef MOZ_WIDGET_GTK2
// whether to check if we're the default mail client on startup (GNOME)
pref("mail.checkDefaultMail", true);
// whether to check if we're the default news client on startup (GNOME)
pref("mail.checkDefaultNews", false);
#endif

/////////////////////////////////////////////////////////////////
// Overrides for generic app behavior from the seamonkey suite's all.js
/////////////////////////////////////////////////////////////////

// l12n and i18n
pref("general.useragent.locale", "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.mailedit", "chrome://global/locale/intl.properties");
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

pref("signon.rememberSignons",              true);
pref("signon.expireMasterPassword",         false);

pref("browser.hiddenWindowChromeURL", "chrome://messenger/content/hiddenWindow.xul");
pref("network.search.url","http://cgi.netscape.com/cgi-bin/url_search.cgi?search=");

pref("general.startup.browser",             false);
pref("general.startup.mail",                false);
pref("general.startup.news",                false);
pref("general.startup.editor",              false);
pref("general.startup.compose",             false);
pref("general.startup.addressbook",         false);

pref("offline.startup_state",            2);
pref("offline.send.unsent_messages",            0);
pref("offline.download.download_messages",  0);
pref("offline.prompt_synch_on_exit",            true);

// Expose only select protocol handlers. All others should go                   
// through the external protocol handler route.                                 
pref("network.protocol-handler.expose-all", false);                             
pref("network.protocol-handler.expose.mailto", true);
pref("network.protocol-handler.expose.news", true);
pref("network.protocol-handler.expose.snews", true);
pref("network.protocol-handler.expose.nntp", true);
pref("network.protocol-handler.expose.imap", true);
pref("network.protocol-handler.expose.addbook", true);
pref("network.protocol-handler.expose.pop", true);                                                                                                            
pref("network.protocol-handler.expose.mailbox", true);  
pref("network.protocols.useSystemDefaults",   false);  
pref("network.hosts.smtp_server",           "mail");
pref("network.hosts.pop_server",            "mail");

pref("general.config.obscure_value", 0); // for MCD .cfg files

pref("xpinstall.dialog.progress", "chrome://communicator/content/xpinstall/xpistatus.xul");
pref("xpinstall.dialog.confirm", "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul");
pref("xpinstall.dialog.progress.type", "");

/////////////////////////////////////////////////////////////////
// End seamonkey suite all.js pref overrides
///////////////////////////////////////////////////////////////// 

/////////////////////////////////////////////////////////////////
// Generic browser related prefs. 
// XXX: Need to scrub these to see which ones thunderbird really needs...
/////////////////////////////////////////////////////////////////

pref("general.open_location.last_url",      "");
pref("general.open_location.last_window_choice", 0);

// 0 = blank, 1 = home (browser.startup.homepage), 2 = last
pref("browser.startup.page",                1);
pref("browser.startup.homepage",	   "chrome://navigator-region/locale/region.properties");
pref("browser.startup.homepage.count", 1);
// "browser.startup.homepage_override" was for 4.x
pref("browser.startup.homepage_override.1", true);
pref("browser.startup.autoload_homepage",   true);

pref("browser.cache.memory.capacity",       4096);

pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.showPopup", true);
pref("browser.urlbar.showSearch", true);
pref("browser.urlbar.matchOnlyTyped", false);

pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", false);

pref("browser.chrome.toolbar_tips",         true);
// 0 = Pictures Only, 1 = Text Only, 2 = Pictures and Text
pref("browser.chrome.toolbar_style",        2);

pref("browser.turbo.enabled", false);

// Dialog modality issues
pref("browser.prefWindowModal", true);
pref("browser.show_about_as_stupid_modal_window", false);

pref("browser.download.progressDnldDialog.keepAlive", true); // keep the dnload progress dialog up after dnload is complete
pref("browser.download.progressDnldDialog.enable_launch_reveal_buttons", true);
pref("browser.download.useDownloadDir", false);
pref("browser.download.folderList", 0);

// various default search settings
pref("browser.search.defaulturl", "chrome://navigator-region/locale/region.properties");
pref("browser.search.opensidebarsearchpanel", true);
pref("browser.search.last_search_category", "NC:SearchCategory?category=urn:search:category:1");
pref("browser.search.mode", 0);
pref("browser.search.powermode", 0);
// basic search popup constraint: minimum sherlock plugin version displayed
// (note: must be a string representation of a float or it'll default to 0.0)
pref("browser.search.basic.min_ver", "0.0");
pref("browser.urlbar.autocomplete.enabled", true);
pref("browser.urlbar.clickSelectsAll", true);

pref("browser.history.grouping", "day");
pref("browser.sessionhistory.max_entries", 50);

// Translation service
pref("browser.translation.service", "http://www.teletranslator.com:8120/?AlisUI=frames_ex/moz_home&alis_info=moz&AlisTargetURI=");
pref("browser.translation.serviceDomain", "teletranslator.com");

// css2 hover pref
pref("nglayout.events.showHierarchicalHover", false);

// Smart Browsing prefs
pref("browser.related.enabled", true);
pref("browser.related.autoload", 1);  // 0 = Always, 1 = After first use, 2 = Never
pref("browser.related.provider", "http://www-rl.netscape.com/wtgn?");
pref("browser.related.disabledForDomains", "");
pref("browser.goBrowsing.enabled", true);

// Default bookmark sorting
pref("browser.bookmarks.sort.direction", "descending");
pref("browser.bookmarks.sort.resource", "rdf:http://home.netscape.com/NC-rdf#Name");

//Internet Search
pref("browser.search.defaultenginename", "chrome://communicator-region/locale/region.properties");

pref("javascript.options.showInConsole",    true);

pref("network.enableIDN",                   false); // Turn on/off IDN (Internationalized Domain Name) resolution
pref("wallet.captureForms",                 true);
pref("wallet.enabled",                      true);
pref("wallet.crypto",                       false);
pref("wallet.crypto.autocompleteoverride",  false); // Ignore 'autocomplete=off' - available only when wallet.crypto is enabled. 
pref("wallet.namePanel.hide",               false);
pref("wallet.addressPanel.hide",            false);
pref("wallet.phonePanel.hide",              false);
pref("wallet.creditPanel.hide",             false);
pref("wallet.employPanel.hide",             false);
pref("wallet.miscPanel.hide",               false);

// -- folders (Mac: these are binary aliases.)
pref("mail.signature_file",             "");
pref("mail.directory",                  "");
pref("news.directory",                  "");
pref("autoupdate.enabled",              true);
pref("browser.editor.disabled", false);
pref("spellchecker.dictionary", "");
pref("profile.allow_automigration", false);   // setting to false bypasses automigration in the profile code
// profile.migration_behavior determines how the profiles root is set
// 0 - use NS_APP_USER_PROFILES_ROOT_DIR
// 1 - create one based on the NS4.x profile root
// 2 - use, if not empty, profile.migration_directory otherwise same as 0 
pref("profile.migration_behavior",0);
pref("profile.migration_directory", "");

// Customizable toolbar stuff
pref("custtoolbar.personal_toolbar_folder", "");

pref("sidebar.customize.all_panels.url", "http://sidebar-rdf.netscape.com/%LOCALE%/sidebar-rdf/%SIDEBAR_VERSION%/all-panels.rdf");
pref("sidebar.customize.directory.url", "http://dmoz.org/Netscape/Sidebar/");
pref("sidebar.customize.more_panels.url", "http://dmoz.org/Netscape/Sidebar/");
pref("sidebar.num_tabs_in_view", 8);

// XXXbsmedberg why are changing the default value here?
// ------------------
//  Numeral Style
// ------------------
// 1 = regularcontextnumeralBidi *
// 2 = hindicontextnumeralBidi
// 3 = arabicnumeralBidi
// 4 = hindinumeralBidi
pref("bidi.numeral", 1);

pref("browser.throbber.url","chrome://navigator-region/locale/region.properties");

// pref to control the alert notification 
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 4000);
pref("alerts.height", 50);

// update notifications prefs
pref("update_notifications.enabled", true);
pref("update_notifications.provider.0.frequency", 7); // number of days
pref("update_notifications.provider.0.datasource", "chrome://communicator-region/locale/region.properties");

// 0 opens the download manager
// 1 opens a progress dialog
// 2 and other values, no download manager, no progress dialog. 
pref("browser.downloadmanager.behavior", 1);

pref("privacy.popups.sound_enabled",              true);
pref("privacy.popups.sound_url",                  "");
pref("privacy.popups.statusbar_icon_enabled",     true);

#ifndef XP_MACOSX
#ifdef XP_UNIX
// Most Unix people think modal pref windows are stupid:
pref("browser.prefWindowModal", false);
// For the download dialog
pref("browser.download.progressDnldDialog.enable_launch_reveal_buttons", false);
pref("browser.urlbar.clickSelectsAll", false);
#endif
#endif
