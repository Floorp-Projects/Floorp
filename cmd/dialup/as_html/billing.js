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



function generateCards()
{
	var theFile = parent.parent.globals.getAcctSetupFilename(self);
	var theCards = parent.parent.globals.GetNameValuePair(theFile,"New Acct Mode","CardTypes");
	if (theCards == null || theCards == "")	{
		theCards = "AX,DC,MC,VI";
		}

	if (theCards.indexOf("AX")>=0)	{
		document.writeln("<OPTION VALUE='American Express'>American Express");
		}
	if (theCards.indexOf("DC")>=0)	{
		document.writeln("<OPTION VALUE='Discover Card'>Discover Card");
		}
	if (theCards.indexOf("MC")>=0)	{
		document.writeln("<OPTION VALUE='MasterCard'>MasterCard");
		}
	if (theCards.indexOf("VI")>=0)	{
		document.writeln("<OPTION VALUE='VISA'>VISA");
		}
}



function loadData()
{
	// make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
	if (((document.forms[0].cardname == "undefined") || (document.forms[0].cardname == "[object InputArray]")) ||
		((document.forms[0].cards == "undefined") || (document.forms[0].cards == "[object InputArray]")) ||
		((document.forms[0].cardnumber == "undefined") || (document.forms[0].cardnumber == "[object InputArray]")) ||
		((document.forms[0].month == "undefined") || (document.forms[0].month == "[object InputArray]")) ||
		((document.forms[0].year == "undefined") || (document.forms[0].year == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	document.forms[0].cardname.value = parent.parent.globals.document.vars.cardname.value;
	if (document.forms[0].cardname.value == "")	{
		document.forms[0].cardname.value = parent.parent.globals.document.vars.first.value + " " + parent.parent.globals.document.vars.last.value;
		}
	for (var i = 0; i<document.forms[0].cards.length; i++)	{
		if (parent.parent.globals.document.vars.cardtype.value == document.forms[0].cards[i].value)	{
			document.forms[0].cards.selectedIndex = i;
			break;
			}
		}
	document.forms[0].cardnumber.value = parent.parent.globals.document.vars.cardnumber.value;
	
	var month = parent.parent.globals.document.vars.month.value;
	if (month == "")	month = 0;
	document.forms[0].month.selectedIndex = month;
	
	var found = false;
	var year = parent.parent.globals.document.vars.year.value;
	// if (year == "")	year = document.forms[0].year[0].value;
	for (var i = 0; i<document.forms[0].year.length; i++)	{
		if (year == document.forms[0].year[i].value)	{
			document.forms[0].year.selectedIndex = i;
			found = true;
			break;
			}
		}
	if (found == false)	{
		document.forms[0].year.selectedIndex = 0;
		}
	parent.parent.globals.setFocus(document.forms[0].cardname);
	if (parent.controls.generateControls)	parent.controls.generateControls();
}



function saveData()
{
	// make sure all form element are valid objects, otherwise just skip & return!
	if (((document.forms[0].cardname == "undefined") || (document.forms[0].cardname == "[object InputArray]")) ||
		((document.forms[0].cards == "undefined") || (document.forms[0].cards == "[object InputArray]")) ||
		((document.forms[0].cardnumber == "undefined") || (document.forms[0].cardnumber == "[object InputArray]")) ||
		((document.forms[0].month == "undefined") || (document.forms[0].month == "[object InputArray]")) ||
		((document.forms[0].year == "undefined") || (document.forms[0].year == "[object InputArray]")))
	{
		parent.controls.reloadDocument();
		return;
	}

	parent.parent.globals.document.vars.cardname.value = document.forms[0].cardname.value;
	if (document.forms[0].cards.length>0 && document.forms[0].cards.selectedIndex >=0)	{
		parent.parent.globals.document.vars.cardtype.value = document.forms[0].cards[document.forms[0].cards.selectedIndex].value;
		}
	else	{
		parent.parent.globals.document.vars.cardtype.value="";
		}
	parent.parent.globals.document.vars.cardnumber.value = document.forms[0].cardnumber.value;
	parent.parent.globals.document.vars.month.value = document.forms[0].month.selectedIndex;
	parent.parent.globals.document.vars.year.value = document.forms[0].year[document.forms[0].year.selectedIndex].value;
}



function checkData()
{
	if (document.forms[0].cardname.value == "")	{
		alert("You must enter a cardholder name.");
		document.forms[0].cardname.focus();
		document.forms[0].cardname.select();
		return false;
		}
	if (document.forms[0].cardnumber.value == "")	{
		alert("You must enter a credit card number.");
		document.forms[0].cardnumber.focus();
		document.forms[0].cardnumber.select();
		return false;
		}
	
	// check cardnumber validity here
	
	var cardNumber=document.forms[0].cardnumber.value;
	for (var i=0; i<cardNumber.length; i++)	{
		if ("1234567890 -".indexOf(cardNumber.substring(i,i+1)) == -1)	{
			alert("'" + cardNumber + "' is not a valid card number.");
			document.forms[0].cardnumber.focus();
			document.forms[0].cardnumber.select();
			return(false);
			}
		}
	
	// do cardnumber check-digit validity here
	
	var length=cardNumber.length;
	var checkDigit=cardNumber.substring(length-1,length);
	var	tempCardNumber="";
	for (var i=0; i<cardNumber.length; i++)	{
		if ("1234567890".indexOf(cardNumber.substring(i,i+1))>=0)	{
			tempCardNumber+=cardNumber.substring(i,i+1);
			}
		}
	var length=tempCardNumber.length;
	var checkSum=0;
	for (var i=0; i<length-1; i++)	{
		var digit=tempCardNumber.substring(length-i-2,length-i-1);
		var temp=digit * (1+((i+1)%2));
		if (temp<10)	checkSum=checkSum+temp;
		else			checkSum=checkSum+(temp-9);
		}
	checkSum=(10-(checkSum%10))%10;
	if (checkSum != checkDigit)	{
		alert("'" + cardNumber + "' is not a valid card number.");
		document.forms[0].cardnumber.focus();
		document.forms[0].cardnumber.select();
		return(false);
		}
	
	// do cardtype check
	
	var cardType="";
	
	if (cardNumber.substring(0,1)=="3" && cardNumber.substring(1,2)=="7")	{
		cardType="American Express";
		cardCode="AX";
		}
/*
	else if (cardNumber.substring(0,1)=="5" && cardNumber.substring(1,2)=="6")	{
		cardType="BankCard";
		cardCode="BC";
		}
*/

/*
	else if (cardNumber.substring(0,1)=="3")	{
		cardType="Diner's Club";
		cardCode="DI";
		}
*/
	else if (cardNumber.substring(0,1)=="5")	{
		cardType="MasterCard";
		cardCode="MC";
		}
	else if (cardNumber.substring(0,1)=="4")	{
		cardType="VISA";
		cardCode="VI";
		}
	else if (cardNumber.substring(0,1)=="6")	{
		cardType="Discover Card";
		cardCode="DC";
		}
// else cardType="unknown";
	
	parent.parent.globals.document.vars.cardcode.value = cardCode;

	if (document.forms[0].cards.length>0)	{
		if (cardType != document.forms[0].cards[document.forms[0].cards.selectedIndex].value)	{
			var found=false;
			var currentSelection = document.forms[0].cards.selectedIndex;
			if (cardType!="")	{
				if (!confirm(cardNumber + " is a " + cardType + ". Correct?"))	{
					document.forms[0].cardnumber.focus();
					document.forms[0].cardnumber.select();
					return(false);
					}
				for (var i = 0; i < document.forms[0].cards.length; i++)	{
					if (document.forms[0].cards[i].value==cardType)	{
						document.forms[0].cards[i].selected=true;
						parent.parent.globals.document.vars.cardtype.value=cardType;
						found=true;
						}
					else	{
						document.forms[0].cards[i].selected=false;
						}
					}
				if (found == false && currentSelection>=0)	{
					document.forms[0].cards[currentSelection].selected=true;
					}
				}
	
			if (found==false)	{
	
				// is the card accepted?
				
				var theFile = parent.parent.globals.getAcctSetupFilename(self);
				var theCards = parent.parent.globals.GetNameValuePair(theFile,"New Acct Mode","CardTypes");
				if (theCards == null || theCards == "")	{
					theCards = "AX,DC,MC,VI";
					}
	
				if (theCards.indexOf(cardCode)<0)	{
					alert(cardType + " is not accepted for payment.");
					}
				else	{
					alert("'" + cardNumber + "' is not a valid card number.");
					}
				document.forms[0].cardnumber.focus();
				document.forms[0].cardnumber.select();
				return(false);
				}
			}

		// check credit card lengths

		var length=tempCardNumber.length;
		var validLength = false;
		if (cardCode != "")	{
			if (cardCode == "AX")	{
				if (length == 16)	validLength=true;
				}
			else if (cardCode == "MC")	{
				if (length == 16)	validLength=true;
				}
			else if (cardCode == "VI")	{
				if (length == 13 || length == 16)	validLength=true;
				}
			else if (cardCode == "DC")	{
				if (length == 16)	validLength=true;
				}
			}
		if (validLength == false)	{
			alert("'" + cardNumber + "' is not a valid card number. (Invalid length)");
			document.forms[0].cardnumber.focus();
			document.forms[0].cardnumber.select();
			return(false);
			}
		}

	// check month and year

	var today = new Date();
	var theMonth = today.getMonth();
	var theYear = today.getYear()+1900;

	var expiredFlag = false;
	if (theYear > document.forms[0].year[document.forms[0].year.selectedIndex].value)	{
		expiredFlag = true;
		}
	else if (theYear == document.forms[0].year[document.forms[0].year.selectedIndex].value)	{
		if (theMonth > document.forms[0].month.selectedIndex)	{
			expiredFlag = true;
			}
		}
	if (expiredFlag)	{
		alert("'" + cardNumber + "' appears to have expired.");
		return(false);
		}

	return true;
}



// end hiding contents from old browsers  -->
