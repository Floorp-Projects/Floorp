# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Thunderbird Default Client Dialog
#
# The Initial Developer of the Original Code is
#   Scott MacGregor <mscott@mozilla.org>
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

// this dialog can only be opened if we have a shell service

var gDefaultClientDialog = {
  onLoad: function () 
  {
    var nsIShellService = Components.interfaces.nsIShellService;
    var shellSvc = Components.classes["@mozilla.org/mail/shell-service;1"]
                             .getService(nsIShellService);
                               
    // initialize the check boxes based on the default app states.
    var mailCheckbox = document.getElementById('checkMail');
    var newsCheckbox = document.getElementById('checkNews');
    var rssCheckbox = document.getElementById('checkRSS');
    
    mailCheckbox.disabled = shellSvc.isDefaultClient(false, nsIShellService.MAIL);
    // as an optimization, if we aren't already the default mail client, then pre-check that option
    // for the user. We'll leave news and RSS alone.
    mailCheckbox.checked = true;
    newsCheckbox.checked = newsCheckbox.disabled = shellSvc.isDefaultClient(false, nsIShellService.NEWS);
    rssCheckbox.checked  = rssCheckbox.disabled  = shellSvc.isDefaultClient(false, nsIShellService.RSS);       
    
    // read the raw pref value and not shellSvc.shouldCheckDefaultMail
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
    document.getElementById('checkOnStartup').checked = prefs.getBoolPref("mail.shell.checkDefaultClient");    
  },
  
  onAccept: function()
  {
    // for each checked item, if we aren't already the default, make us the default.
    var nsIShellService = Components.interfaces.nsIShellService;    
    var shellSvc = Components.classes["@mozilla.org/mail/shell-service;1"]
                             .getService(nsIShellService);
    var appTypes = 0;                            
    if (document.getElementById('checkMail').checked && !shellSvc.isDefaultClient(false, nsIShellService.MAIL))
      appTypes |= nsIShellService.MAIL;
    if (document.getElementById('checkNews').checked && !shellSvc.isDefaultClient(false, nsIShellService.NEWS))
      appTypes |= nsIShellService.NEWS;
    if (document.getElementById('checkRSS').checked &&  !shellSvc.isDefaultClient(false, nsIShellService.RSS))
      appTypes |= nsIShellService.RSS;
    
    if (appTypes)
      shellSvc.setDefaultClient(false, appTypes);

    shellSvc.shouldCheckDefaultClient = document.getElementById('checkOnStartup').checked;
  }
};
