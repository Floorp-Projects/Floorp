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
 * The Original Code is mozilla.
 *
 * The Initial Developer of the Original Code is
 * Srilatha Moturi <srilatha@netscape.com>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rajiv Dayal <rdayal@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

var gCurrentDirectoryPrefName;
var gReplicationService = Components.classes["@mozilla.org/addressbook/ldap-replication-service;1"].getService(Components.interfaces.nsIAbLDAPReplicationService);
var gProgressText;
var gProgressMeter;
var gReplicationBundle;

function onLoad() 
{
  var dirName;

  if (window.arguments[0]) {
    dirName = window.arguments[0].dirName;
    gCurrentDirectoryPrefName = window.arguments[0].prefName;
  }

  gProgressText = document.getElementById("replication.status");
  gProgressMeter = document.getElementById("replication.progress");
  gReplicationBundle = document.getElementById("bundle_replication");

  document.title = gReplicationBundle.getFormattedString("replicatingTitle", [dirName])

  Replicate();
}

var progressListener = {
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_START) {
      // start the spinning
      gProgressMeter.setAttribute("mode","undetermined");
      if (aStatus)
        SetProgressText(gReplicationBundle.getString("replicationStarted"));
      else
        SetProgressText(gReplicationBundle.getString("changesStarted"));
    }
    
    if (aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
      // stop the spinning
      gProgressMeter.setAttribute("mode","normal");
      gProgressMeter.setAttribute("value", "100");

      if (aStatus)
        SetProgressText(gReplicationBundle.getString("replicationSucceeded"));
      else
        SetProgressText(gReplicationBundle.getString("replicationFailed"));
      window.close();
    }
  },
  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress)
  {
    SetProgressText(gReplicationBundle.getFormattedString("currentCount", [aCurSelfProgress]));     
  },
  onLocationChange: function(aWebProgress, aRequest, aLocation)
  {
  },
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage)
  {
  },
  onSecurityChange: function(aWebProgress, aRequest, state)
  {
  },
  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIWebProgressListener) || 
        iid.equals(Components.interfaces.nsISupportsWeakReference) || 
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
};

function Replicate()
{
  try {
    gReplicationService.startReplication(gCurrentDirectoryPrefName, progressListener);
  }
  catch (ex) {
    // XXX todo
    // are you offline?  If so, you need to go offline to use this
    dump("replication failed.  ex=" + ex + "\n");
  }
}

function onCancelReplication()
{
  try {
    // XXX todo, confirm the cancel
    gReplicationService.cancelReplication(gCurrentDirectoryPrefName);
  }
  catch (ex) {
    // XXX todo
    // perhaps replication hasn't started yet?  This can happen if you hit cancel after attempting to replication when offline 
    dump("unexpected failure while cancelling.  ex=" + ex + "\n");
  }
  return true;
}

function SetProgressText(textStr)
{
  gProgressText.value = textStr;
}
