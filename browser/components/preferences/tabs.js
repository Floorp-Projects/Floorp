# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Jeff Walden <jwalden+code@mit.edu>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gTabsPane = {

  /*
   * Preferences:
   *
   * browser.link.open_external
   * - determines where pages opened by external applications are opened:
   *     0 opens such links in the default window,
   *     1 opens such links in the most recent window or tab,
   *     2 opens such links in a new window,
   *     3 opens such links in a new tab
   * browser.link.open_newwindow
   * - determines where pages which would open in a new window are opened:
   *     0 opens such links in the default window,
   *     1 opens such links in the most recent window or tab,
   *     2 opens such links in a new window,
   *     3 opens such links in a new tab
   * browser.tabs.autoHide
   * - true if the tab bar is hidden when only one tab is open, false to always
   *   show it
   * browser.tabs.loadInBackground
   * - true if display should switch to a new tab which has been opened from a
   *   link, false if display shouldn't switch
   * browser.tabs.warnOnClose
   * - true if when closing a window with multiple tabs the user is warned and
   *   allowed to cancel the action, false to just close the window
   * browser.tabs.warnOnOpen
   * - true if the user should be warned if he attempts to open a lot of tabs at
   *   once (e.g. a large folder of bookmarks), false otherwise
   */

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns 2 if such links should be opened in new windows,
   *          3 if such links should be opened in new tabs
   */
  readLinkTarget: function() {
    var openExternal = document.getElementById("browser.link.open_external");
    return openExternal.value != 2 ? 3 : 2;
  },

  /**
   * Ensures that pages opened in new windows by web pages and pages opened by
   * external applications both open in the same way (e.g. in a new tab, window,
   * etc.).
   *
   * @returns 2 if such links should be opened in new windows,
   *          3 if such links should be opened in new tabs
   */
  writeLinkTarget: function() {
    var linkTargeting = document.getElementById("linkTargeting");
    document.getElementById("browser.link.open_newwindow").value = linkTargeting.value;
    return linkTargeting.value;
  }
};

