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
 * The Original Code is Mozilla Communicator Test Cases..
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   radha@netscape.com (original author)
 *   depstein@netscape.com
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

/********************************************************
nsISHistoryListener object
*********************************************************/

function sessionHistoryListener()
{
   ddump("In sessionHistoryListener constructor\n");
   this.interfaceName = "nsISHistoryListener";

}

// implementations of SHistoryListener methods
sessionHistoryListener.prototype =
{
    result: "",
    debug: 1,

    QueryInterface: function(aIID)
    {
        netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
        netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
        if (aIID.equals(Components.interfaces.nsISHistoryListener) ||
            aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
            aIID.equals(Components.interfaces.nsISupports))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    OnHistoryNewEntry: function(newUrl)
    {
       netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
       uriSpec = newUrl.spec;

       ddump("In OnHistoryNewEntry(). uriSpec = " + uriSpec + "\n");
    },

    OnHistoryGoBack: function(uri)
    {
       uriSpec = uri.spec;

       ddump("In OnHistoryGoBack(). uriSpec = " + uriSpec + "\n");
       backCallback = true;
       return true;
    },

    OnHistoryGoForward: function(uri)
    {
       uriSpec = uri.spec;

       ddump("In OnHistoryGoForward(). uriSpec = " + uriSpec + "\n");
       forwardCallback = true;
       return true;
    },

    OnHistoryReload: function(uri, reloadFlags)
    {
       uriSpec = uri.spec;

       ddump("In OnHistoryReload(). uriSpec = " + uriSpec + "\n");
       ddump("In OnHistoryReload(). reloadFlags = " + reloadFlags + "\n");
       reloadCallback = true;
       return true;
    },

    OnHistoryGotoIndex: function(index, uri)
    {
       uriSpec = uri.spec;

       ddump("In OnHistoryGotoIndex(). uriSpec = " + uriSpec + "\n");
       ddump("In OnHistoryGotoIndex(). index = " + index + "\n");
       gotoCallback = true;
       return true;
    },

    OnHistoryPurge: function(numEntries)
    {
       ddump("In OnHistoryPurge(). numEntries = " + numEntries + "\n");
       purgeCallback = true;
       return true;
    },

    ddump: function(s)
    {
       if (debug ==1)
          dump(s);
       else if (debug==2)
          alert(s);
    }
}


