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
 * The Original Code is Chimera.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

// This file contains Camino-specific default preferences.

pref("accessibility.tabfocus", 3);

// What to load in a new tab: 0 = blank, 1 = homepage, 2 = last page
pref("browser.tabs.startPage", 0);

pref("browser.quartz.enable.all_font_sizes", true);

pref("chimera.store_passwords_with_keychain", true);

// by default, Camino will take proxy settings (including PAC) from the OS.
// set this pref to false if you want to specify proxy settings by hand.
pref("camino.use_system_proxy_settings", true);

pref("camino.enable_plugins", true);

// show warning when closing windows or quitting
pref("camino.warn_when_closing", true);

// show warning when attempting to open a feed to another application
pref("camino.warn_before_opening_feed", true);

// turn off dumping of JS and CSS errors to the console
pref("chimera.log_js_to_console", false);

// Identify Camino in the UA string
pref("general.useragent.vendor", "Camino");
pref("general.useragent.vendorSub", "1.2+");

pref("browser.chrome.favicons", true);
pref("browser.urlbar.autocomplete.enabled", true);

// Default to auto download enabled but auto helper dispatch disabled
pref("browser.download.autoDownload", true);
pref("browser.download.autoDispatch", false);
pref("browser.download.progressDnldDialog.keepAlive", true);

// Download removing policy
pref("browser.download.downloadRemoveAction", 0);

// set typeahead find to search all text by default, but not invoke
// unless you type '/'
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.autostart", false);

// image resizing
pref("browser.enable_automatic_image_resizing", true);

// Languages (note we try to override this with the data from System Preferences,
// this is just the emergency fall-back)
pref("intl.accept_languages", "en-us, en" );

// Scrolling overrides.  For vertical: control for size, option for history.
// Shift, command, and modifierless scroll the document.  If it's a
// shift-scroll that comes from a mouse that only scrolls vertically, we get
// it from the system as a (shifted) horizontal scroll.
pref("mousewheel.withcontrolkey.action", 3);
pref("mousewheel.withcontrolkey.sysnumlines", false);
pref("mousewheel.withaltkey.action", 2);
pref("mousewheel.withaltkey.sysnumlines", false);

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

// defaults for cmd-click opening windows or tabs (default to windows)
pref("browser.tabs.opentabfor.middleclick", false);
pref("browser.tabs.loadInBackground", false);

// use the html network errors rather than sheets
pref("browser.xul.error_pages.enabled", true);

// map delete key to back button
pref("browser.backspace_action", 0);

// bring download window to the front when each download starts
pref("browser.download.progressDnldDialog.bringToFront", true);

// 0 = spellcheck nothing, 1 = check multi-line controls, 2 = check 
//  multi/single line controls
pref("layout.spellcheckDefault", 2);

// enable the tab jumpback feature
pref("camino.enable_tabjumpback", true);

// set the window.open behavior to "respect SWM unless the window.open call specifies size or features"
pref("browser.link.open_newwindow.restriction", 2);

// enable popup blocking
pref("dom.disable_open_during_load", true);

// don't hide user:pass when fixing up URIs
pref("browser.fixup.hide_user_pass", false);

// give users the option of restoring windows after a crash
pref("browser.sessionstore.resume_from_crash", true);
