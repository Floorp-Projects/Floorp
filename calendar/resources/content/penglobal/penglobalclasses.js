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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Colin Phillips <colinp@oeone.com>
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


var MEDIATOR_CONTRACTID = "@mozilla.org/rdf/datasource;1?name=window-mediator";
var nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
var rootwindowID = "rootwindow";

function pencontrols()
{
	this.root = rootwindowID;
}

function penapplication()
{
	this.application = "calendar";
	this.appnum = this.getRoot().Root.getAppNum(this.application);
	this.penDialogModalWindow = null;
	this.penDialogWindows = new Array();
	this.penDialogArray = new Array();
}
penapplication.prototype = new pencontrols;

function pendialog()
{
	this.dialog = null;
	this.sticky = false;
	this.xcoord = null;
	this.ycoord = null;
   if (opener)
   {
		this.application = opener.penapplication.application;
      this.appnum = opener.penapplication.appnum;
   }
	else
	{
		this.application = rootwindowID;
      this.appnum = -1;
	}
}
pendialog.prototype = new pencontrols;

/*--------------------------------------------------
* Supporting functions for each of the above
*-------------------------------------------------*/

//--controls--------------------------------------------

pencontrols.prototype.getRoot = function ()
{
	var windowManager = Components.classes[MEDIATOR_CONTRACTID].getService();
	var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
	var enumerator = windowManagerInterface.getEnumerator( null );

	while ( enumerator.hasMoreElements()  )
	{
	   var nextWindow = enumerator.getNext();
	   var domWindow = windowManagerInterface.convertISupportsToDOMWindow( nextWindow );
	   if (domWindow.name == rootwindowID)
	   {
			return domWindow;
		}
	}
	return self;
}

pencontrols.prototype.getAppPath = function (AppName)
{
	var windowManager = Components.classes[MEDIATOR_CONTRACTID].getService();
	var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
	var enumerator = windowManagerInterface.getEnumerator( null );
	var RootWin = null;

	while ( enumerator.hasMoreElements()  )
	{
	   var nextWindow = enumerator.getNext();
	   var domWindow = windowManagerInterface.convertISupportsToDOMWindow( nextWindow );
		if (domWindow.penapplication)
		{
			if (domWindow.penapplication.application == AppName)
			{
				return domWindow;
			}
		}
		else if (domWindow.name == rootwindowID)
		{
			RootWin = domWindow;
		}
	}
	if (RootWin != null)
	{
		existsInRoot = RootWin.Root.getRootWindowAppPath(AppName);
		if (existsInRoot != null)
		{
			return existsInRoot;
		}
	}
	return null;
}

pencontrols.prototype.getDialogWindow = function (DialogName)
{
	var windowManager = Components.classes[MEDIATOR_CONTRACTID].getService();
	var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
	var enumerator = windowManagerInterface.getEnumerator( null );

	DialogName = "dialogWindow" + DialogName;

	while ( enumerator.hasMoreElements()  )
	{
	   var nextWindow = enumerator.getNext();
	   var domWindow = windowManagerInterface.convertISupportsToDOMWindow( nextWindow );
		if (domWindow.pendialog)
		{
			if (domWindow.pendialog.dialog == DialogName)
			{
				return domWindow;
			}
		}
	}
	return null;
}

//--application--------------------------------------------

penapplication.prototype.getApp = function ()
{
	return self;
}

penapplication.prototype.getDialogEntry = function (id)
{
	for (NumDialogsOpen = 0; NumDialogsOpen < this.penDialogWindows.length; NumDialogsOpen++)
	{
		if (id == this.penDialogWindows[NumDialogsOpen])
		{
			return NumDialogsOpen;
		}
	}
	return null;
}

penapplication.prototype.getOpenDialogEntry = function ()
{
	for (NumDialogsOpen = 0; NumDialogsOpen < this.penDialogWindows.length; NumDialogsOpen++)
	{
		if (this.penDialogWindows[NumDialogsOpen] == null)
		{
			return NumDialogsOpen;
		}
	}
	return this.penDialogWindows.length;
}

penapplication.prototype.closeDialogEntry = function (id)
{
	closeSlot = this.getDialogEntry(id);
	if (closeSlot == null)
	{
		dump("id '" + id + "' is invalid");
		return;
	}
	this.penDialogWindows[closeSlot] = null;
	this.penDialogArray[closeSlot] = null;
}

penapplication.prototype.openDialog = function (id, file, modal, penarguments)
{
	id = "dialogWindow" + id;
	if ((modal)&&(this.penDialogModalWindow != null))
	{
		return;
	}
	else if (this.getDialogEntry(id) != null)
	{
		return;
	}
	if (modal)
	{
		if (penarguments)
		{
         this.penDialogModalWindow = true;
			window.openDialog(file, id, "chrome,centerscreen,alwaysRaised,modal", id, penarguments);
		}
		else
		{
         this.penDialogModalWindow = true;
			window.openDialog(file, id, "chrome,centerscreen,alwaysRaised,modal", id);
		}
	}
	else
	{
		nextOpenSlot = this.getOpenDialogEntry();
		this.penDialogWindows[nextOpenSlot] = id;
		if (penarguments)
		{
			this.penDialogArray[nextOpenSlot] = window.openDialog(file, id, "chrome,centerscreen,alwaysRaised", id, penarguments);
		}
		else
		{
			this.penDialogArray[nextOpenSlot] = window.openDialog(file, id, "chrome,centerscreen,alwaysRaised", id);
		}
	}
}

penapplication.prototype.closeDialog = function (id)
{
	if (this.penDialogModalWindow)
	{
		this.penDialogModalWindow = null;
	}
	else
	{
		this.closeDialogEntry(id)
	}
}

penapplication.prototype.showThisDialog = function (dialogHandle)
{
	userDialogs = penapplication.mozuserpath + "/userDialogs.xml";
   listOfNeverShowDialogs = penFileUtils.loadFromXML( userDialogs, "dialog" );
	if ((listOfNeverShowDialogs != "")&&(listOfNeverShowDialogs.length > 0))
	{
		for (neverShowLength = 0; neverShowLength < listOfNeverShowDialogs.length; neverShowLength++)
		{
			if (listOfNeverShowDialogs[neverShowLength].name == dialogHandle)
			{
				return false;
			}
		}
	}
	return true;
}

penapplication.prototype.getDialogPath = function (id)
{
	if (this.penDialogModalWindow)
	{
		return this.penDialogModalWindow;
	}
	else
	{
		id = "dialogWindow" + id;
		return this.getInternalDialogPath(id);
	}
}

penapplication.prototype.getInternalDialogPath = function (id)
{
	if (this.penDialogModalWindow)
	{
		return this.penDialogModalWindow;
	}
	else
	{
		Slot = this.getDialogEntry(id)
		if (Slot == null)
		{
			return null;
		}
		return this.penDialogArray[Slot];
	}
}

//--dialog-------------------------------------------------

pendialog.prototype.getApp = function ()
{
	return opener;
}

pendialog.prototype.closeDialog = function (id)
{
	if ((this.getApp())&&(this.getApp().penapplication))
	{
		this.getApp().penapplication.closeDialog(id);
	}
}

//--other functions----------------------------------------

function debug( Text )
{
   dump( "\n"+ Text + "\n");
}
