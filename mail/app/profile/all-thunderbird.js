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

#expand pref("general.useragent.extra.thunderbird", "Thunderbird/__APP_VERSION__");

#expand pref("general.useragent.locale", "__AB_CD__");
pref("general.skins.selectedSkin", "classic/1.0");

#ifdef XP_MACOSX
pref("browser.chromeURL", "chrome://messenger/content/messengercompose/messengercompose.xul");
pref("mail.biff.animate_dock_icon", false);
#endif

pref("update.app.enabled", true); // Whether or not app updates are enabled 
pref("update.app.url", "chrome://mozapps/locale/update/update.properties");	
pref("update.extensions.enabled", true);

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
pref("app.update.vendorName.override", "Mozilla");

// URL user can browse to manually if for some reason all update installation
// attempts fail.  TODO: Change this URL
pref("app.update.url.manual", "http://www.mozilla.org/products/thunderbird/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "chrome://messenger-region/locale/region.properties");
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

// Blocklist preferences
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/1/%APP_ID%/%APP_VERSION%/");
pref("extensions.blocklist.detailsURL", "http://www.mozilla.com/blocklist/");

// Developers can set this to |true| if they are constantly changing files in their
// extensions directory so that the extension system does not constantly think that
// their extensions are being updated and thus reregistered every time the app is started
pref("extensions.ignoreMTimeChanges", false);
// Enables some extra Extension System Logging (can reduce performance) 
pref("extensions.logging.enabled", false); 

// Symmetric (can be overridden by individual extensions) update preferences.
// e.g.
//  extensions.{GUID}.update.enabled
//  extensions.{GUID}.update.url
//  extensions.{GUID}.update.interval
//  .. etc ..
//
pref("extensions.update.enabled", true);
pref("extensions.update.autoUpdateEnabled", true);
pref("extensions.update.url", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.update.autoUpdate", false);    // Automatically download and install 
                                                // updates to themes and extensions. 
                                                // Does nothing at present. 

pref("extensions.update.interval", 86400);  // Check for updates to Extensions and 
                                            // Themes every week
// Non-symmetric (not shared by extensions) extension-specific [update] preferences
pref("extensions.getMoreExtensionsURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.getMoreThemesURL", "chrome://mozapps/locale/extensions/extensions.properties");
pref("extensions.update.severity.threshold", 5);// The number of pending Extension/Theme
                                                // updates you can have before the update
                                                // notifier goes from low->medium severity.
pref("extensions.update.count", 0);             // The number of extension/theme/etc 
                                                // updates available
pref("extensions.dss.enabled", false);          // Dynamic Skin Switching                                               
pref("extensions.dss.switchPending", false);    // Non-dynamic switch pending after next
                                                // restart.


// General Update preferences
pref("update.interval", 3600000);               // Check each of the above intervals 
                                                // every 60 mins
pref("update.showSlidingNotification", true);   // Windows-only slide-up taskbar 
                                                // notification.
// These prefs relate to the number and severity of updates available. This is a 
// cache that the browser notification mechanism uses to determine if it should show
// status bar UI if updates are detected and the app is shut down before installing
// them.
// 0 = low    (extension/theme updates), 
// 1 = medium (numerous extension/theme updates), 
// 2 = high   (new version of Firefox/Security patch)
pref("update.severity", 0); 

pref("xpinstall.whitelist.add", "update.mozilla.org");
pref("xpinstall.whitelist.add.103", "addons.mozilla.org");

pref("mail.phishing.detection.enabled", true); // enable / disable phishing detection for link clicks
pref("mail.spellcheck.inline", true);
pref("mail.showPreviewText", true); // enables preview text in mail alerts and folder tooltips

// Folder Pane View
// 0 == All Folders
// 1 == Unread Folders
// 2 == Favorite Folders
// 3 == Most Recently Used Folders
pref("mail.ui.folderpane.view", 0);
pref("mail.folder.views.version", 0);

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

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.flashBar", 1);

/////////////////////////////////////////////////////////////////
// Overrides of the seamonkey suite mailnews.js prefs
///////////////////////////////////////////////////////////////// 
pref("mail.showFolderPaneColumns", false); // setting to true will allow total/unread/size columns
pref("mail.showCondensedAddresses", true); // show the friendly display name for people I know

/////////////////////////////////////////////////////////////////
// Privacy Controls for Handling Remote Content
///////////////////////////////////////////////////////////////// 
pref("mailnews.message_display.allow.plugins", false); // disable plugins by default
pref("mailnews.message_display.disable_remote_image", true);
pref("mailnews.message_display.disable_remote_images.useWhitelist", true);
pref("mailnews.message_display.disable_remote_images.whiteListAbURI","moz-abmdbdirectory://abook.mab");

/////////////////////////////////////////////////////////////////
// Trusted Mail Domains
//
// Specific domains can be white listed to bypass various privacy controls in Thunderbird
// such as blocking remote images, the phishing detector, etc. This is particularly
// useful for business deployments where images or links reference servers inside a 
// corporate intranet. For multiple domains, separate them with a comma. i.e.
// pref("mail.trusteddomains", "mozilla.org,mozillafoundation.org");
///////////////////////////////////////////////////////////////// 
pref("mail.trusteddomains", "");

// hidden pref for changing how we present attachments in the message pane
pref("mailnews.attachments.display.largeView", false); 
pref("mail.pane_config.dynamic",            0);
pref("mailnews.reuse_thread_window2",     true);
pref("mail.spam.display.sanitize", true); // sanitize the HTML in spam messages
pref("mail.standalone", true); 
pref("editor.singleLine.pasteNewlines", 4);  // substitute commas for new lines in single line text boxes

// hidden pref to ensure a certain number of headers in the message pane
// to avoid the height of the header area from changing when headers are present / not present
pref("mailnews.headers.minNumHeaders", 0); // 0 means we ignore this pref

pref("mail.compose.dontWarnMail2Newsgroup", false);

pref("network.cookie.cookieBehavior", 3); // 0-Accept, 1-dontAcceptForeign, 2-dontUse, 3-p3p

/////////////////////////////////////////////////////////////////
// End seamonkey suite mailnews.js pref overrides
///////////////////////////////////////////////////////////////// 

// whether to check if we are the default mail, news or feed client
// on startup. 
pref("mail.checkDefaultMail", true);
pref("mail.checkDefaultNews", false);
pref("mail.checkDefaultFeed", false);

/////////////////////////////////////////////////////////////////
// Overrides for generic app behavior from the seamonkey suite's all.js
/////////////////////////////////////////////////////////////////

// l12n and i18n
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
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");

pref("signon.rememberSignons",              true);
pref("signon.expireMasterPassword",         false);
pref("signon.SignonFileName",               "signons.txt");

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

// suppress external-load warning for standard browser schemes
pref("network.protocol-handler.warn-external.http", false);
pref("network.protocol-handler.warn-external.https", false);
pref("network.protocol-handler.warn-external.ftp", false);

pref("network.hosts.smtp_server",           "mail");
pref("network.hosts.pop_server",            "mail");

pref("security.warn_entering_secure.show_once", false);
pref("security.warn_entering_weak.show_once", false);
pref("security.warn_leaving_secure.show_once", false);
pref("security.warn_viewing_mixed.show_once", false);

pref("general.config.obscure_value", 0); // for MCD .cfg files

pref("xpinstall.dialog.confirm", "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul");
pref("xpinstall.dialog.progress.skin", "chrome://mozapps/content/extensions/extensions.xul?view=installs");
pref("xpinstall.dialog.progress.chrome", "chrome://mozapps/content/extensions/extensions.xul?view=installs");
pref("xpinstall.dialog.progress.type.skin", "Extension:Manager"); 
pref("xpinstall.dialog.progress.type.chrome", "Extension:Manager");

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

pref("browser.send_pings", false);

pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.showPopup", true);
pref("browser.urlbar.showSearch", true);
pref("browser.urlbar.matchOnlyTyped", false);

pref("browser.chrome.site_icons", true);
pref("browser.chrome.favicons", false);

pref("browser.chrome.toolbar_tips",         true);
// 0 = Pictures Only, 1 = Text Only, 2 = Pictures and Text
pref("browser.chrome.toolbar_style",        2);

pref("browser.xul.error_pages.enabled", true);

// Attachment download manager settings
pref("mail.attachment.store.version", 0);
pref("browser.download.useDownloadDir", false);
pref("browser.download.folderList", 0);
pref("browser.download.manager.showAlertOnComplete", false);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 1);
pref("browser.download.manager.showWhenStarting", true);
pref("browser.download.manager.useWindow", true);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 100);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 0);

// various default search settings
pref("browser.search.defaulturl", "chrome://navigator-region/locale/region.properties");
pref("browser.search.opensidebarsearchpanel", true);
pref("browser.search.last_search_category", "NC:SearchCategory?category=urn:search:category:1");
pref("browser.search.mode", 0);
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
// profile.force.migration can be used to bypass the migration wizard, forcing migration from a particular
// mail application without any user intervention. Possible values are: 
// dogbert (4.x), seamonkey (mozilla suite), eudora, oexpress, outlook. 
pref("profile.force.migration", "");

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

// prefs to control the mail alert notification
pref("alerts.slideIncrementTime", 50);
pref("alerts.totalOpenTime", 3000);

// 0 opens the download manager
// 1 opens a progress dialog
// 2 and other values, no download manager, no progress dialog. 
pref("browser.downloadmanager.behavior", 1);

pref("privacy.popups.sound_enabled",              true);
pref("privacy.popups.sound_url",                  "");
pref("privacy.popups.statusbar_icon_enabled",     true);

#ifndef XP_MACOSX
#ifdef XP_UNIX
// For the download dialog
pref("browser.download.progressDnldDialog.enable_launch_reveal_buttons", false);
pref("browser.urlbar.clickSelectsAll", false);
#endif
#endif

// prevent status-bar spoofing even if people are foolish enough to turn on JS
pref("dom.disable_window_status_change",          true);
