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
	return(true);
}



function verifyCallWaiting(cwData)
{
	var	validFlag=true;

	if (cwData.length > 0)	{
		for (var x=0; x<cwData.length; x++)	{
			if ("0123456789*#,".indexOf(cwData.charAt(x)) <0)	{
				validFlag=false;
				break;
				}
			}
		}
	return(validFlag);
}



function verifyPrefix(prefixData)
{
	var	validFlag=true;

	if (prefixData.length > 0)	{
		for (var x=0; x<prefixData.length; x++)	{
			if ("0123456789,".indexOf(prefixData.charAt(x)) <0)	{
				validFlag=false;
				break;
				}
			}
		}
	return(validFlag);
}



function checkData()
{
	if (verifyCallWaiting(document.forms[0].cwData.value) == false)	{
		alert("Please enter a valid call waiting string. (It may contain numbers, asterisks, pound signs and commas.)");
		parent.parent.globals.setFocus(document.forms[0].cwData);
		return(false);
		}
	if (verifyPrefix(document.forms[0].prefixData.value) == false)	{
		alert("Please enter a valid outside line string. (It may contain numbers and commas.)");
		parent.parent.globals.setFocus(document.forms[0].prefixData);
		return(false);
		}
	return(true);
}



function loadData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].cwData == "undefined") || (document.forms[0].cwData == "[object InputArray]")) ||
		((document.forms[0].cwOptions == "undefined") || (document.forms[0].cwOptions == "[object InputArray]")) ||
		((document.forms[0].prefixData == "undefined") || (document.forms[0].prefixData == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].cwData.value=parent.parent.globals.document.forms[0].cwData.value;
//	document.forms[0].cwOFF.checked=parent.parent.globals.document.forms[0].cwOFF.checked;
//	document.forms[0].cwOFF.checked=(document.forms[0].cwData.value=="") ? 0:1;

	var found=false;
	for (var i = 0; i < document.forms[0].cwOptions.length; i++)	{
		if (document.forms[0].cwOptions[i].value==parent.parent.globals.document.forms[0].cwData.value)	{
			document.forms[0].cwOptions[i].selected=true;
			found=true;
			}
		else	{
			document.forms[0].cwOptions[i].selected=false;
			}
		}
	if (found==false)	{
		if (document.forms[0].cwData.value == "")	{
			document.forms[0].cwOptions[0].text = "(Line Doesn't Have Call Waiting)";
			document.forms[0].cwOptions[0].value = "";
			}
		else	{
			document.forms[0].cwOptions[0].text = document.forms[0].cwData.value;
			document.forms[0].cwOptions[0].value = document.forms[0].cwData.value;
			}
		document.forms[0].cwOptions[0].selected=true;
		}
	document.forms[0].prefixData.value=parent.parent.globals.document.forms[0].prefixData.value;
//	document.forms[0].prefix.checked=parent.parent.globals.document.forms[0].prefix.checked;
//	document.forms[0].prefix.checked=(document.forms[0].prefixData.value=="") ? 0:1;

	var theModem = parent.parent.globals.document.vars.modem.value;
	var theModemType = parent.parent.globals.document.setupPlugin.GetModemType(theModem);
	if (theModemType != null)	{
		theModemType = theModemType.toUpperCase();
		if (theModemType == "ISDN")	{
			// do nothing
			}
		else	{
			if (parent.parent.globals.document.forms[0].dialMethod.value == "PULSE")	{
				document.layers["dialingMethods"].document.forms[0].dialMethod[0].checked=false;
				document.layers["dialingMethods"].document.forms[0].dialMethod[1].checked=true;
				}
			else	{

				document.layers["dialingMethods"].document.forms[0].dialMethod[0].checked=true;
				document.layers["dialingMethods"].document.forms[0].dialMethod[1].checked=false;
				}
			}
		}
	parent.parent.globals.setFocus(document.forms[0].cwData);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].cwData == "undefined") || (document.forms[0].cwData == "[object InputArray]")) ||
		((document.forms[0].cwOptions == "undefined") || (document.forms[0].cwOptions == "[object InputArray]")) ||
		((document.forms[0].prefixData == "undefined") || (document.forms[0].prefixData == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

//	parent.parent.globals.document.forms[0].cwOFF.checked = document.forms[0].cwOFF.checked;
	parent.parent.globals.document.forms[0].cwData.value = document.forms[0].cwData.value;
//	parent.parent.globals.document.forms[0].prefix.checked = document.forms[0].prefix.checked;
	parent.parent.globals.document.forms[0].prefixData.value = document.forms[0].prefixData.value;

	var theModem = parent.parent.globals.document.vars.modem.value;
	var theModemType = parent.parent.globals.document.setupPlugin.GetModemType(theModem);
	if (theModemType != null)	{
		theModemType = theModemType.toUpperCase();
		if (theModemType == "ISDN")	{
			parent.parent.globals.document.forms[0].dialMethod.value = theModemType;
			}
		else	{
			if (document.layers["dialingMethods"].document.forms[0].dialMethod[1].checked == true)	{
				parent.parent.globals.document.forms[0].dialMethod.value = document.layers["dialingMethods"].document.forms[0].dialMethod[1].value;
				}
			else	{
				parent.parent.globals.document.forms[0].dialMethod.value = document.layers["dialingMethods"].document.forms[0].dialMethod[0].value;
				}
			}
		}
}



function updateCWOptions(theObject)
{
/*
	if (theObject.name=="cwOFF")	{
		if (theObject.checked)	{
			parent.parent.globals.setFocus(document.forms[0].cwData);
			}
		else	{
			document.forms[0].cwData.value="";
			document.forms[0].cwOFF.checked=0;
			}
		}
	else
*/
	if (theObject.name=="cwData")	{
		document.forms[0].cwOptions[0].text = "(Line Doesn't Have Call Waiting)";
		document.forms[0].cwOptions[0].value = "";
		if (document.forms[0].cwData.value=="")	{
			document.forms[0].cwOptions.selectedIndex = 0;
			}
		else	{
			var found=0;
			for (var x=1; x<document.forms[0].cwOptions.length; x++)	{
				if (document.forms[0].cwOptions[x].text == document.forms[0].cwData.value)	{
					found=x;
					break;
					}
				}
			if (found<1)	{
				document.forms[0].cwOptions[0].text = document.forms[0].cwData.value;
				document.forms[0].cwOptions[0].value = document.forms[0].cwData.value;
				}
			document.forms[0].cwOptions.selectedIndex = found;
			}
		}
	else
	 if (theObject.name=="cwOptions")	{
		document.forms[0].cwData.value=document.forms[0].cwOptions[document.forms[0].cwOptions.selectedIndex].value;
		parent.parent.globals.setFocus(document.forms[0].cwData);
//		document.forms[0].cwOFF.checked=1;
		}
	return(true);
}



function updatePrefix(theObject)
{
/*
	if (theObject.name=="prefix")	{
		if (theObject.checked)	{
			parent.parent.globals.setFocus(document.forms[0].prefixData);
			}
		else	{
			document.forms[0].prefixData.value="";
			document.forms[0].prefix.checked=0;
			}
		}
	else if (theObject.name=="prefixData")	{
		if (document.forms[0].prefixData.value=="")	{
			document.forms[0].prefix.checked=0;
			}
		else	{
			document.forms[0].prefix.checked=1;
			}
		}
*/
	return(false);
}



function generateDialingMethods()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	var theModem = parent.parent.globals.document.vars.modem.value;
	var theModemType = parent.parent.globals.document.setupPlugin.GetModemType(theModem);
	if (theModemType != null && theModemType.toUpperCase() != "ISDN")	
	{
		document.layers["dialingMethods"].visibility = "show";
	}
	else
	{
		document.layers["dialingMethods"].visibility = "hide";
	}
}



// end hiding contents from old browsers  -->
