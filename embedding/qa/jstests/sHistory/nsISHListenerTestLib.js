/*   Descrpt: test Sesion History interfaces
     Author: radha@netscape.com
     Revs: 06.27.01 - Created

   - The contents of this file are subject to the Mozilla Public
   - License Version 1.1 (the "License"); you may not use this file
   - except in compliance with the License. You may obtain a copy of
   - the License at http://www.mozilla.org/MPL/
   -
   - Software distributed under the License is distributed on an "AS
   - IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
   - implied. See the License for the specific language governing
   - rights and limitations under the License.
   -
   - The Original Code is Mozilla Communicator Test Cases.
   -
   - The Initial Developer of the Original Code is Netscape Communications
   - Corp.  Portions created by Netscape Communications Corp. are
   - Copyright (C) 1999 Netscape Communications Corp.  All
   - Rights Reserved.
   -
   - Contributor(s):
   	depstein@netscape.com
*/

/********************************************************
nsISHistoryListener object
*********************************************************/

function sessionHistoryListener()
{
   alert("In sessionHistoryListener constructor");
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
        throw Components.results.NS_NOINTERFACE;
    },

    OnHistoryNewEntry: function(newUrl)
    {
       netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
       uriSpec = newUrl.spec;

       alert("In OnHistoryNewEntry(). uriSpec = " + uriSpec);
    },

    OnHistoryGoBack: function(uri)
    {
       uriSpec = uri.spec;

       alert("In OnHistoryGoBack(). uriSpec = " + uriSpec);
       backCallback = true;
       return true;
    },

    OnHistoryGoForward: function(uri)
    {
       uriSpec = uri.spec;

       alert("In OnHistoryGoForward(). uriSpec = " + uriSpec);
       forwardCallback = true;
       return true;
    },

    OnHistoryReload: function(uri, reloadFlags)
    {
       uriSpec = uri.spec;

       alert("In OnHistoryReload(). uriSpec = " + uriSpec);
       alert("In OnHistoryReload(). reloadFlags = " + reloadFlags);
       reloadCallback = true;
       return true;
    },

    OnHistoryGotoIndex: function(index, uri)
    {
       uriSpec = uri.spec;

       alert("In OnHistoryGotoIndex(). uriSpec = " + uriSpec);
       alert("In OnHistoryGotoIndex(). index = " + index);
       gotoCallback = true;
       return true;
    },

    OnHistoryPurge: function(numEntries)
    {
       alert("In OnHistoryPurge(). numEntries = " + numEntries);
       purgeCallback = true;
       return true;
    },

    ddump: function(s)
    {
       if (debug ==1)
          dump(s);
       else
          alert(s);
    }
}


