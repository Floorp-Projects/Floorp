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


