/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
<!--  to hide script contents from old browsers



function go(msg)
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")
		return true;
	else
		return(checkData());
}



function checkData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (document.forms[0].modem.selectedIndex >= 0) {
		if (parent.parent.globals.document.vars.path.value == "New Path")	{
			var theModem = document.forms[0].modem[document.forms[0].modem.selectedIndex].value;
/*
			var theModemType = parent.parent.globals.document.setupPlugin.GetModemType(theModem);
			if (theModemType != null)	{
				theModemType = theModemType.toUpperCase();
				if (theModemType == "ISDN")	{
					alert("ISDN modems can not be used to connect to the Internet account server.");
					return(false);
					}
				}
*/
			if (theModem != "")	{
				if (theModem.indexOf("ISDN-")>=0)	{			// magic "ISDN-" check
					alert("ISDN modems can not be used to connect to the Internet account server.");
					return(false);
					}
				}

			}
		}
	else	{
		alert("Please select a modem, or install a modem if no modem is installed!");
		return(false);
		}

	return(true);
}



function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);

	if (thePlatform != "WinNT")	{
		document.layers["ModemSetup"].visibility = "show";
		}


	updateModemStatus(true);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (document.forms[0].modem.selectedIndex >= 0) {
		parent.parent.globals.document.vars.modem.value = document.forms[0].modem[document.forms[0].modem.selectedIndex].value;
	}

	parent.parent.globals.document.setupPlugin.CloseModemWizard();
}



function generateModems()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var modemList = parent.parent.globals.document.setupPlugin.GetModemList();
	var thePopup = document.forms[0]["modem"];

	if (modemList != null && thePopup) 
	{
		//remove all old options
		for (var i = (thePopup.length -1); i >= 0 ; i--)
		{
			thePopup.options[i] = null;
		}	

		//add the modems
		for(var index = 0; index < modemList.length; index++)
		{
			thePopup.options[thePopup.options.length] = new Option(modemList[index],modemList[index], false, false);
		}
		
		//select the current modem
		selectCurrentModem();

	}
}


function selectCurrentModem()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var found = false;
	var thePopup = document.forms[0]["modem"];
	var globalModem = parent.parent.globals.document.vars.modem.value;
	var pluginModem = parent.parent.globals.document.setupPlugin.GetCurrentModemName();

	var thePlatform = new String(navigator.userAgent);
	var x=thePlatform.indexOf("(")+1;
	var y=thePlatform.indexOf(";",x+1);
	thePlatform=thePlatform.substring(x,y);

	var selectIndex = 0;
	if (thePlatform != "Macintosh")	{  // work around for window's list index bug
		var selectIndex = thePopup.options.length-1;
		}

	//alert("globalModem: " + globalModem + ", pluginModem: " + pluginModem);
	for(var index = 0; index < thePopup.options.length; index++)
	{
		if ((globalModem == thePopup.options[index]) || (selectIndex == 0 && pluginModem == thePopup.options[index]) )
			selectIndex = index;
	}

	thePopup.options[selectIndex].selected = true;	
}

function OLDgenerateModems()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var modemList = parent.parent.globals.document.setupPlugin.GetModemList();

	if (modemList != null) {
		var theModem = parent.parent.globals.document.vars.modem.value;
		var selectedStr = "";

		for (var x=0; x<modemList.length; x++)	{
			if (modemList[x] == theModem)	{
				selectedStr=" SELECTED";
				}
			else	{
				selectedStr="";
				}
			document.writeln("<OPTION VALUE='" + modemList[x] + "'" + selectedStr + ">" + modemList[x]);
			}
	}
}



function updateModemStatus(loadingFlag)
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (parent.parent.globals.document.setupPlugin.IsModemWizardOpen() == true)	{
		setTimeout("updateModemStatus(false)",1000);
		}
	else	{
		for (x=document.forms[0].modem.length-1; x>=0; x--)	{
			document.forms[0].modem.options[x]=null;
			}

	if (loadingFlag == false)	{
		var selectedModem=parent.parent.globals.document.setupPlugin.GetCurrentModemName();
		if (selectedModem != null && selectedModem != "")	{
			parent.parent.globals.document.vars.modem.value = selectedModem;
			}
		}

	var theModem = parent.parent.globals.document.setupPlugin.GetModemList();
	if (theModem != null)	{
		var theSelectedIndex=-1;
		for (x=0; x<theModem.length; x++)	{
			var selectedFlag = (parent.parent.globals.document.vars.modem.value==theModem[x]);
		if (selectedFlag==true)	theSelectedIndex=x;
			document.forms[0].modem.options[x] = new Option(theModem[x],theModem[x],selectedFlag,selectedFlag);
			}
		if (theSelectedIndex>=0)	{
			document.forms[0].modem.selectedIndex=theSelectedIndex;
			}
		}
	
	//generateModems();
	
	}
}



function callModemWizard()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (document.forms[0].modem.selectedIndex >= 0) {
		parent.parent.globals.document.vars.modem.value = document.forms[0].modem[document.forms[0].modem.selectedIndex].value;
	}
	parent.parent.globals.document.setupPlugin.OpenModemWizard();
	setTimeout("updateModemStatus(false)",1000);
	return(false);
}



// end hiding contents from old browsers  -->
