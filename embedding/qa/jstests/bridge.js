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
 *   Ashish Bhatt <ashishbhatt@netscape.com>
 *
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


/*******************************************************
WriteResults()
********************************************************/
function WriteResults(buffer)
{

    var obj = getAppObj();
    if (!obj)
    {
       alert("Cannot use this function, You are not runnig this test case in automation framework");
       return ;
    }
    obj.WriteResults(buffer) ;
}

/*******************************************************
isRunningStandalone()
********************************************************/
function isRunningStandalone()
{
   var obj = getAppObj();
   if(obj)
     return false ;
   else
     return true ;
}


/*******************************************************
getAppObj()
********************************************************/
function getAppObj()
{
   var obj ;

   const WIN_MEDIATOR_CTRID =
   	  "@mozilla.org/appshell/window-mediator;1";
   const nsIWindowMediator    = Components.interfaces.nsIWindowMediator;

   netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserAccess");
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    var windowManager =
           Components.classes[WIN_MEDIATOR_CTRID].getService(nsIWindowMediator);
    var parentwindow = windowManager.getMostRecentWindow("Automation:framewrk");

    if (parentwindow)
    {
	   obj =  parentwindow.document.getElementById("ContentBody");
	   return obj.appobj
    }

   if (window.opener)
   {
		obj = window.opener.document.getElementById("ContentBody");
		if (obj)
		   return obj.appobj;
   }
   obj =  document.getElementById("ContentBody");

   if (obj)
	  return obj.appobj;

    var win = window.top;
    while(win)
    {
       obj = win.document.getElementById("ContentBody");
       if (obj)
		  return obj.appobj ;

       if (win == win.top)
         return null ;

       win = win.parent ;
    }
   return null ;
}
