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



var	intlString = "";
var	localString = "";



var theFile = parent.parent.globals.getAcctSetupFilename(self);
var intlFlag = parent.parent.globals.GetNameValuePair(theFile,"Mode Selection","IntlMode");
intlFlag = intlFlag.toLowerCase();

if (intlFlag == "yes")	{
	intlString = "text";
	localString = "hidden";
	}
else	{
	intlString = "hidden";
	localString = "text";
	}



function Country(name,countryCode)
{
	this.name=name;
	this.countryCode=countryCode;
}


var countryList=new Array();
countryList[0] =new Country("Australia","61");
countryList[1] =new Country("Austria","43");
countryList[2] =new Country("Belgium","32");
countryList[3] =new Country("Canada","1");
countryList[4] =new Country("Denmark","45");
countryList[5] =new Country("Finland","358");
countryList[6] =new Country("France","33");
countryList[7] =new Country("Germany","49");
countryList[8] =new Country("Great Britain","44");
countryList[9] =new Country("Greece","30");
countryList[10]=new Country("Hong Kong","852");
countryList[11]=new Country("Iceland","354");
countryList[12]=new Country("Indonesia","62");
countryList[13]=new Country("Ireland","353");
countryList[14]=new Country("Italy","39");
countryList[15]=new Country("Japan","81");
countryList[16]=new Country("Malaysia","60");
countryList[17]=new Country("Netherlands","31");
countryList[18]=new Country("New Zealand","64");
countryList[19]=new Country("Norway","47");
countryList[20]=new Country("Philippines","63");
countryList[21]=new Country("Singapore","65");
countryList[22]=new Country("Spain","34");
countryList[23]=new Country("Sweden","46");
countryList[24]=new Country("Switzerland","41");
countryList[25]=new Country("USA","1");



function writeLocalText(theString)
{
	if (localString == "text")	{
		document.write(theString);
		}
}



function generateCountryList()
{
if (intlFlag == "yes")	{
	var	country = parent.parent.globals.document.vars.country.value;

	document.writeln("<TR><TD COLSPAN='3'><spacer type=vertical size=2></TD></TR>");
	document.writeln("<TR><TD VALIGN=MIDDLE ALIGN=RIGHT HEIGHT=25><B>Country:</B></TD><TD ALIGN=LEFT VALIGN=TOP COLSPAN=2>");
	document.writeln("<SELECT NAME='countryList'>");
	for (var x=0; x<countryList.length; x++)	{
		var selected=(country==countryList[x].name) ? " SELECTED":"";
		document.writeln("<OPTION VALUE='" + countryList[x].name + "'" + selected + ">" + countryList[x].name);
		}
	document.writeln("</SELECT></TD></TR>");
	}
}



function go(msg)
{
	if (parent.parent.globals.document.vars.editMode.value == "yes")
		return true;
	else
		return(checkData());
}



function checkData()
{
	if (document.forms[0].first.value == "")	{
		alert("You must enter a first name.");
		document.forms[0].first.focus();
		document.forms[0].first.select();
		return(false);
		}
	if (document.forms[0].last.value == "")	{
		alert("You must enter a last name.");
		document.forms[0].last.focus();
		document.forms[0].last.select();
		return(false);
		}
	if (document.forms[0].address1.value == "")	{
		alert("You must enter a street address.");
		document.forms[0].address1.focus();
		document.forms[0].address1.select();
		return(false);
		}

	if (intlFlag != "yes")	{
		if (document.forms[0].city.value == "")	{
			alert("You must enter a city.");
			document.forms[0].city.focus();
			document.forms[0].city.select();
			return(false);
			}
		if (document.forms[0].state.value == "")	{
			alert("You must enter a state or province.");
			document.forms[0].state.focus();
			document.forms[0].state.select();
			return(false);
			}
		if (document.forms[0].state.value.length < 2)	{
			alert("You must enter a valid state or province.");
			document.forms[0].state.focus();
			document.forms[0].state.select();
			return(false);
			}
		if (document.forms[0].zip.value == "")	{
			alert("You must enter a ZIP or postal code.");
			document.forms[0].zip.focus();
			document.forms[0].zip.select();
			return(false);
			}
		if (parent.parent.globals.verifyZipCode(document.forms[0].zip.value)==false)	{
			alert("Please enter a valid ZIP or postal code.");
			parent.parent.globals.setFocus(document.forms[0].zip);
			return(false);
			}
		if (document.forms[0].areaCode.value == "")	{
			alert("You must enter an area code.");
			document.forms[0].areaCode.focus();
			document.forms[0].areaCode.select();
			return(false);
			}
		if (parent.parent.globals.verifyAreaCode(document.forms[0].areaCode.value)==false)	{
			alert("Please enter a valid area code.");
			parent.parent.globals.setFocus(document.forms[0].areaCode);
			return(false);
			}
		}
	if (document.forms[0].phoneNumber.value == "")	{
		alert("You must enter a telephone number.");
		document.forms[0].phoneNumber.focus();
		document.forms[0].phoneNumber.select();
		return(false);
		}
	if (parent.parent.globals.verifyPhoneNumber(document.forms[0].phoneNumber.value)==false)	{
		alert("Please enter a valid telephone number.");
		parent.parent.globals.setFocus(document.forms[0].phoneNumber);
		return(false);
		}
	return(true);
}



function loadData()
{
	// make sure all form element are valid objects, otherwise reload the page!
	if (((document.forms[0].first == "undefined") || (document.forms[0].first == "[object InputArray]")) ||
		((document.forms[0].last == "undefined") || (document.forms[0].last == "[object InputArray]")) ||
		((document.forms[0].company == "undefined") || (document.forms[0].company == "[object InputArray]")) ||
		((document.forms[0].address1 == "undefined") || (document.forms[0].address1 == "[object InputArray]")) ||
		((document.forms[0].address2 == "undefined") || (document.forms[0].address2 == "[object InputArray]")) ||
		((document.forms[0].address3 == "undefined") || (document.forms[0].address3 == "[object InputArray]")) ||
		((document.forms[0].city == "undefined") || (document.forms[0].city == "[object InputArray]")) ||
		((document.forms[0].state == "undefined") || (document.forms[0].state == "[object InputArray]")) ||
		((document.forms[0].zip == "undefined") || (document.forms[0].zip == "[object InputArray]")) ||
		((document.forms[0].areaCode == "undefined") || (document.forms[0].areaCode == "[object InputArray]")) ||
		((document.forms[0].phoneNumber == "undefined") || (document.forms[0].phoneNumber == "[object InputArray]")))
	{
		top.globals.debug("FORM ELEMENT: " + document.forms[0].first);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].last);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].company);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address1);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address2);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address3);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].city);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].state);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].zip);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].areaCode);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].phoneNumber);
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].first.value = parent.parent.globals.document.vars.first.value;
	document.forms[0].last.value = parent.parent.globals.document.vars.last.value;
	document.forms[0].company.value = parent.parent.globals.document.vars.company.value;
	document.forms[0].address1.value = parent.parent.globals.document.vars.address1.value;
	document.forms[0].address2.value = parent.parent.globals.document.vars.address2.value;
	document.forms[0].address3.value = parent.parent.globals.document.vars.address3.value;
	document.forms[0].city.value = parent.parent.globals.document.vars.city.value;
	document.forms[0].state.value = parent.parent.globals.document.vars.state.value;
	document.forms[0].zip.value = parent.parent.globals.document.vars.zip.value;
	document.forms[0].areaCode.value = parent.parent.globals.document.vars.areaCode.value;
	document.forms[0].phoneNumber.value = parent.parent.globals.document.vars.phoneNumber.value;
	parent.parent.globals.setFocus(document.forms[0].first);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].first == "undefined") || (document.forms[0].first == "[object InputArray]")) ||
		((document.forms[0].last == "undefined") || (document.forms[0].last == "[object InputArray]")) ||
		((document.forms[0].company == "undefined") || (document.forms[0].company == "[object InputArray]")) ||
		((document.forms[0].address1 == "undefined") || (document.forms[0].address1 == "[object InputArray]")) ||
		((document.forms[0].address2 == "undefined") || (document.forms[0].address2 == "[object InputArray]")) ||
		((document.forms[0].address3 == "undefined") || (document.forms[0].address3 == "[object InputArray]")) ||
		((document.forms[0].city == "undefined") || (document.forms[0].city == "[object InputArray]")) ||
		((document.forms[0].state == "undefined") || (document.forms[0].state == "[object InputArray]")) ||
		((document.forms[0].zip == "undefined") || (document.forms[0].zip == "[object InputArray]")) ||
		((document.forms[0].areaCode == "undefined") || (document.forms[0].areaCode == "[object InputArray]")) ||
		((document.forms[0].phoneNumber == "undefined") || (document.forms[0].phoneNumber == "[object InputArray]")))
	{
		top.globals.debug("SAVE DATA....");
		top.globals.debug("FORM ELEMENT: " + document.forms[0].first);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].last);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].company);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address1);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address2);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].address3);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].city);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].state);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].zip);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].areaCode);
		top.globals.debug("FORM ELEMENT: " + document.forms[0].phoneNumber);

		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.first.value = document.forms[0].first.value;
	parent.parent.globals.document.vars.last.value = document.forms[0].last.value;
	parent.parent.globals.document.vars.company.value = document.forms[0].company.value;
	parent.parent.globals.document.vars.address1.value = document.forms[0].address1.value;
	parent.parent.globals.document.vars.address2.value = document.forms[0].address2.value;
	parent.parent.globals.document.vars.address3.value = document.forms[0].address3.value;
	parent.parent.globals.document.vars.city.value = document.forms[0].city.value;
	parent.parent.globals.document.vars.state.value = document.forms[0].state.value;
	parent.parent.globals.document.vars.zip.value = document.forms[0].zip.value;
	parent.parent.globals.document.vars.areaCode.value = document.forms[0].areaCode.value;
	parent.parent.globals.document.vars.phoneNumber.value = document.forms[0].phoneNumber.value;

	if (intlFlag == "yes")	{
		var theCountry = document.forms[0].countryList.options[document.forms[0].countryList.selectedIndex].text;
		for (var x=0; x<countryList.length; x++)	{
			if (theCountry == countryList[x].name)	{
				parent.parent.globals.document.vars.country.value = countryList[x].name;
				parent.parent.globals.document.vars.countryCode.value = countryList[x].countryCode;
				break;
				}
			}
		}
	else	{
		parent.parent.globals.document.vars.country.value = "USA";
		parent.parent.globals.document.vars.countryCode.value = "1";
		}
}



// end hiding contents from old browsers  -->
