<!--  -*- Mode: HTML; tab-width: 2; indent-tabs-mode: nil -*-

   The contents of this file are subject to the Netscape Public License
   Version 1.0 (the "NPL"); you may not use this file except in
   compliance with the NPL.  You may obtain a copy of the NPL at
   http://www.mozilla.org/NPL/

   Software distributed under the NPL is distributed on an "AS IS" basis,
   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
   for the specific language governing rights and limitations under the
   NPL.

   The Initial Developer of this code under the NPL is Netscape
   Communications Corporation.  Portions created by Netscape are
   Copyright (C) 1998 Netscape Communications Corporation.  All Rights
   Reserved.
 -->
//toolbar.js


//handles:
//Globals frame: opener.top.globals
//controls frame: opener.top.screen.controls

// Request privilege
compromisePrincipals();			// work around for the security check
netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
//var theEditor = null;
var ready;
var cfgHelpWindow;
var numValidPopupEntries = 0;

function finishedLoading()
{
	if (ready && document.layers["reload"])
		return true;
	else
		return false;
}

function getLayerVisibility(inLayerName)
{
	if (finishedLoading && document.layers[inLayerName])
		return document.layers[inLayerName].visibility;
}


function editScreen(isHelpScreen) 
{
		
		var thePlatform = new String(navigator.userAgent);
		var x=thePlatform.indexOf("(")+1;
		var y=thePlatform.indexOf(";",x+1);
		var helpFolderAppend = "help/";
		var macFSHelpFolderAppend = "help:";
		thePlatform=thePlatform.substring(x,y);

		// Request privilege

		netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

		var theEditor = top.opener.top.globals.document.vars.externalEditor.value;
		//alert("editor: " + theEditor);
		
		var theLoc = null;
		
		
		
		//see if we can find a help Window that is open, and snarf it's location
		if (top.opener.top.screen.controls.helpWindow && top.opener.top.screen.controls.helpWindow != null && !top.opener.top.screen.controls.helpWindow.closed && top.opener.top.screen.controls.helpWindow.location)
		{
			theLoc = new String(top.opener.top.screen.controls.helpWindow.location);
			helpFolderAppend = "";  // the path will already point to the help folder if we get it from the window.
			macFSHelpFolderAppend = "";
		}
		else
		{
			//default case - no help window open, figure out it's location as we normally would.
			theLoc = new String(opener.top.screen.content.location);
		}
		
		//if we should look for the help file, parse that location here
		if (isHelpScreen && isHelpScreen != null && isHelpScreen == true)
		{
			var defaultHelpFile = "ashelp.htm";
			var slashIdx = theLoc.lastIndexOf("/");
			var thePath = theLoc.substring(0,slashIdx+1);
			var thePage = theLoc.substring(slashIdx+1, theLoc.length);
			
			if (thePath.substring(thePath.length - 6, thePath.length) == "intro/")
			{
				thePath = thePath.substring(0, thePath.length - 6);
			}
			else if (thePath.substring(thePath.length - 9, thePath.length) == "ipreview/")
			{
				thePath = thePath.substring(0, thePath.length - 9);
			}
			else if (thePath.substring(thePath.length - 8, thePath.length) == "preview/")
			{
				thePath = thePath.substring(0, thePath.length - 8);
			}
						
			var helpFile = thePath + helpFolderAppend + defaultHelpFile;
			
			//now get a directory listing and look for a help file that matches the
			// thePage fileName
			if (top.opener && top.opener.top.globals)	
			{
				helpPath = "" + top.opener.top.globals.getFolder(top.opener.top.globals);
			}

			if (thePlatform == "Macintosh")	
			{						// Macintosh support
				helpPath = helpPath + macFSHelpFolderAppend;
			}
			else	
			{												// Windows support
				helpPath = helpPath + helpFolderAppend;
			}
	
			var lookingForFile = thePage;
			
			var theList = top.opener.top.globals.document.setupPlugin.GetFolderContents(helpPath,".htm");
			if (theList != null)	
			{
				for (var i=0; i<theList.length; i++)	
				{
					if (lookingForFile == theList[i])	{
						helpFile = thePath + helpFolderAppend + lookingForFile;
						break;
						}
				}
			}
			//else
			//	alert("found no list from: " + helpPath);
				
			if (helpFile != null && helpFile != "null" && helpFile != "")
				theLoc = helpFile;
			
			//alert("HelpLoc: " + helpFile);
				
		}
		//now, if the layer select popup is around, see if it has a better location for us
		else if ((!isHelpScreen) && (document.layers["layerSwitch"]) && (document.layers["layerSwitch"].visibility=="show") && (document.layers["layerSwitch"].document.forms) && (document.layers["layerSwitch"].document.forms[0]["layerSelect"]))
		{
			var selindex = document.layers["layerSwitch"].document.forms[0]["layerSelect"].options.selectedIndex;
			var theValue =  document.layers["layerSwitch"].document.forms[0]["layerSelect"].options[selindex].value;
			
			if (theValue && theValue != null && theValue != "" && theValue != "null"  && theValue != "_none")
			{
				document.layers["layerSwitch"].document.forms[0]["layerSelect"].options[selindex].value;
				theLoc = theValue;
				//alert("Would edit: " + theValue);
			}
			else
			{
				//alert("ERROR: selected layer had a defined location of: " + theValue);	
			}
		}

		

		if (theEditor.toString().lastIndexOf("Netscape Communicator") >= 0)	
		{
			//editWindow=window.open("","_blank","dependent=yes,toolbar=1,location=0,directories=0,status=1,menubar=1,scrollbars=1,resizable=1");
			//editWindow.onerror=null;
			//editWindow.location=theLoc;
			netscape.plugin.composer.Document.editDocument(theLoc);
		}
		else if (theEditor != null && theEditor != "" && theEditor.length > 0)
		{
		
			var theFile = theLoc;
			
			//We need to see if we are editing a multi-layer document, which has the layer popup, and
			//if so, make sure we get the right document from the popup here
			
			
			if (theFile.indexOf("file:///") ==0)	{
				theFile = theFile.substring(8,theFile.length);
				}

			if (thePlatform == "Macintosh")	{
				var path=unescape(theFile);
				var fileArray=path.split("/");
				var newpath=fileArray.join(":");
				if (newpath.charAt(0)==':')	{
					newpath=newpath.substring(1,newpath.length);
					}
				theFile=unescape(newpath);			
				}
			else	{
				// note: JavaScript returns path with '/' instead of '\'        
				var path=unescape(theFile);
				                
				// gets the drive letter and directory path
				var Drive = path.substring(path.indexOf('|')-1, path.indexOf('|'));
				var thepath = path.substring(path.indexOf('/'), path.length);
				var newpath=Drive + ":" + thepath;
				var fileArray=newpath.split("/");
				theFile=fileArray.join("\\");
				}

			//alert("I wanna open: " + theFile);
			opener.top.globals.document.setupPlugin.OpenFileWithEditor(theEditor, theFile);
		}
		else
		{
			alert("You must select an application as your editor before you can edit HTML pages.");
			chooseEditor();
		}
			
}


function setShowScreenBox()
{
	if (document && document.layers["showscreen"] && document.layers["showscreen"].document.ssForm)
	{	
		var theBox = document.layers["showscreen"].document.ssForm.showScreenBox;
		if ((theBox))
		{
		
			var isVisible = screenVisible();
			
			if (isVisible == false)
			{
				theBox.checked = false;
			}
			else
			{
				theBox.checked = true;
			}
		}
	}
}


function screenVisible()
{
	var visible = true;
	
	if (opener.top && opener.top.screen && opener.top.screen.controls && opener.top.screen.controls.screenVisible)
	{
			visible = opener.top.screen.controls.screenVisible();
	}
	//else
	//alert("toolbar - screenvisible: " + visible);
	
	return visible;
}

function reloadScreen()
{
	//var theLoc = top.opener.screen.content.location;
	//alert("top.opener.screen.content.location is " + top.opener.screen.content.location);
	//top.opener.screen.content.history.go(0);
	top.opener.screen.controls.reloadDocument();
	return false;
}

function chooseEditor()
{

	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");
	
	var theEditor = opener.top.globals.document.setupPlugin.GetExternalEditor();
	if ((theEditor != null) && (theEditor != "")) {
		top.opener.top.globals.document.vars.externalEditor.value = theEditor;
		top.opener.top.globals.saveExternalEditor();
	}
}

function screenOptions()
{
	return opener.top.screen.controls.go('Screen Options');

}

function showScreen(thisRef)
{
	var result = thisRef.checked;
	
	var theBox = document.layers["showscreen"].document.ssForm.showScreenBox;
	if ((theBox))
	{
		var result = opener.top.screen.controls.showScreen(theBox.checked);
		if ((result != null)) 
			theBox.checked = result;
	}
	
	return result;
}

function hidelayer(layerName)
{
	if (document.layers)
	{
		theLayer = eval("document.layers."+layerName);
		if(theLayer)
		{
			theLayer.visibility="hide";
		}
	}
	else
	{
		//alert("hide: " + layerName);
		theImg = eval("document.images."+layerName);
		replaceSrc(theImg, "Images/blank.gif");
		replaceSrc(theImg, "Images/blank1.gif");
	}
}

function showlayer(layerName)
{
	if (document.layers)
	{
		theLayer = eval("document.layers."+layerName);
		if(theLayer)
		{
			theLayer.visibility="show";
		}
	}
	else
	{
		//alert("show: " + layerName);
		theImg = eval("document.images."+layerName);
		if(theImg)
			replaceSrc(theImg, theImg.lowsrc);	
	}
}


function toggleShow(lName, checkValue)
{
	//alert(checkValue);
	if ((checkValue != null) && (checkValue == false))
		hidelayer(lName);
	else
		showlayer(lName);

}

function callback()
{
	ready = true;
	opener.top.screen.controls.generateToolBarControls();
}

function updateLayersLayer()
{
	//first, evaluate the ## of layers in the top document.
	var numLayers = 0;
	var thePopup = document.layers["layerSwitch"].document.forms[0]["layerSelect"];
	
	if (thePopup && !top.loading)
	{
		//if (top.opener.screen.content.document.layers && top.opener.screen.content.document.layers.length > 0)
		//	numLayers = top.opener.screen.content.document.layers.length;
		numLayers = top.opener.screen.controls.countDocumentLayers();
	
		if (numLayers > 0)
		{
			document.layers["layerSwitch"].visibility="show";
			var curLayerName="", curLayerSrc="_none";
			
	
			//blank out old list
top.opener.top.globals.debug("deleting options list: " + thePopup.options.length + " present");
		
			for (var i = (numValidPopupEntries -1); i >= 0 ; i--)
			{
				thePopup.options[i] = new Option(" "," ",false,false);
			}	
			//thePopup.options.length = 0;

			//add a layer for the main body
			thePopup.options[0] = new Option("Main Document                     .",top.opener.screen.controls.getDocumentLocation(), false, false);
			numValidPopupEntries = 1;
top.opener.top.globals.debug("setting popuocount to 1, options.length is: " + thePopup.options.length);

			for(var index = 0; index < numLayers; index++)
			{
				curLayerName=top.opener.screen.controls.getLayerName(index);
				curLayerSrc=top.opener.screen.controls.getLayerSrc(index);
				if (!curLayerSrc || curLayerSrc == null || curLayerSrc == "null" || curLayerSrc == "")
					 curLayerSrc = "_none";				  
				
				if (top.opener.screen.controls.getDocumentLayerVisibility(index) == "hide")
				{
					curLayerName = (curLayerName + " [hidden]");
				}
				
				if (curLayerName == null || curLayerName == "")
				{
					curLayerName = ("Layer " + eval(index+1));
				} 
				
				//alert("Layer name: " + curLayerName + "; src: " + curLayerSrc);
				
				if ((top.opener.screen.controls.getDocumentLayerVisibility(index) == "hide") || ((curLayerSrc != null) && (curLayerSrc != "") && (curLayerSrc != "null") && (curLayerSrc != "_none")))
				{
top.opener.top.globals.debug("Adding layer " + curLayerName + " to options " + numValidPopupEntries + ".  thePopup.options.length now: " + thePopup.options.length);
					thePopup.options[numValidPopupEntries] = new Option(curLayerName,curLayerSrc, false, false);
					numValidPopupEntries++;
				}
			}	
			thePopup.selectedIndex=0;
			document.layers["layerSwitch"].document.layers["g_hideothers"].visibility= "hide"; //hide the checkbox if the main document is selected.
		}
		else
		{
			hidelayer("layerSwitch");
			document.layers["layerSwitch"].document.layers["g_hideothers"].visibility= "hide"; //hide the checkbox if the main document is selected.

		}
	}
	else
		setTimeout("updateLayersLayer()",1000);
	
}


function selectLayer(popupIndex)
{

	var numLayers = numValidPopupEntries;//document.layers["layerSwitch"].document.forms[0]["layerSelect"].options.length;
	var layerName = "";
	var hideCheckBox = 	null;
	
	if (popupIndex >= numValidPopupEntries)
	{
		popupIndex = 0;
		document.layers["layerSwitch"].document.forms[0]["layerSelect"].selectedIndex = 0;
	}
	if (document && document.layers && document.layers["layerSwitch"] && document.layers["layerSwitch"].document.forms[0] && document.layers["layerSwitch"].document.layers["g_hideothers"].document.forms[0]["hideLayers"])
		hideCheckBox = document.layers["layerSwitch"].document.layers['g_hideothers'].document.forms[0]["hideLayers"];

	if (top.opener.screen.controls.countDocumentLayers() > 0)
	{

		//first restore all originally hidden layers to their old hidden state:
		for (var layidx = 1; layidx < numLayers; layidx++)
		{
			//alert("Layer " + layidx + "/" + numLayers);
			var hiddenIndex = document.layers["layerSwitch"].document.forms[0]["layerSelect"].options[layidx].text.toString().indexOf(" [hidden]");
			
			if (hiddenIndex >= 0)
			{
				var layerName = document.layers["layerSwitch"].document.forms[0]["layerSelect"].options[layidx].text.substring(0, hiddenIndex);
				//alert("hiding " + layerName);
				//if (top.opener.screen.content.document.layers[layerName].visibility)
					top.opener.top.screen.controls.showDocumentLayer(layerName, false);
					//top.opener.screen.content.document.layers[layerName].visibility="hide";
			}
		}
		
		//now make sure we are showing the currently selected layer
		if (popupIndex > 0)				//omit 0 becuase that represents the main document
		{
top.opener.top.globals.debug("selectLayer, showing current Layer #: " + popupIndex);

			document.layers["layerSwitch"].document.layers["g_hideothers"].visibility="show";
			layerName = document.layers["layerSwitch"].document.forms[0]["layerSelect"].options[popupIndex].text.toString();
top.opener.top.globals.debug("selectLayer, layer name is: " + layerName);
			if (layerName.indexOf(" [hidden]") > 0)
				layerName = layerName.substring(0,layerName.indexOf(" [hidden]"));
			top.opener.top.screen.controls.showDocumentLayer(layerName, true);
		}
		else if (hideCheckBox != null && hideCheckBox.checked) //showing main document
		{
			numLayers = top.opener.screen.controls.countDocumentLayers();
			//show all document layers
	 		for (var index = 0; index < numLayers; index++)
			{
				layerName=top.opener.screen.controls.getLayerName(index);
				
				if (!checkIfHiddenInLayerPopup(layerName))
					top.opener.top.screen.controls.showDocumentLayer(layerName, true);
			
				hideCheckBox.checked = false;
			}				
		}
		
		if (popupIndex == 0)
			document.layers["layerSwitch"].document.layers["g_hideothers"].visibility= "hide"; //hide the checkbox if the main document is selected.

		

	}
}


function checkIfHiddenInLayerPopup(inLayerName)
{
	var thePopup = document.layers["layerSwitch"].document.forms[0]["layerSelect"]; 
	var hiddenName = inLayerName + " [hidden]";
	var result = false;
	
	if (thePopup && thePopup != null)
	{
top.opener.top.globals.debug("length: " + thePopup.length + ".  options.length: " + thePopup.options.length);
		for (var index = thePopup.length-1; index > 0; index--)
		{
			if (thePopup.options[index].text.toString() == hiddenName)
			{
				result = true;
				return result;
			}
		}
	}
	
	return result;
}

//show all layers that were originally not hidden.
function restoreLayers()
{
	//first, evaluate the ## of layers in the top document.
	var numLayers = 0;
	var curLayerName = "";
	
	numLayers = top.opener.screen.controls.countDocumentLayers();

	for (var index = 0; index < numLayers; index++)
	{
		curLayerName=top.opener.screen.controls.getLayerName(index);
		
		//if (!checkIfHiddenInLayerPopup(curLayerName))
			top.opener.top.screen.controls.showDocumentLayer(curLayerName, true);
	}	

	selectLayer(document.layers["layerSwitch"].document.forms[0]["layerSelect"].selectedIndex);
}


function hideOtherLayers(inDoHide)
{

	var thePopup = document.layers["layerSwitch"].document.forms[0]["layerSelect"];
	
	if (!thePopup || thePopup == null)
		return;

	if (inDoHide == false)
		restoreLayers();
	else if (thePopup.selectedIndex > 0)
	{
		//first, evaluate the ## of layers in the top document.
		var numLayers = 0;
		var curLayerName = "";
top.opener.top.globals.debug("hideOtherLayers, getting layer name for: " + thePopup.selectedIndex);
		var selectedLayerName = thePopup.options[thePopup.selectedIndex].text.toString();
top.opener.top.globals.debug("hideOtherLayers, layer name is: " + selectedLayerName);
		
		numLayers = top.opener.screen.controls.countDocumentLayers();
	
		for (var index = 0; index < numLayers; index++)
		{
			curLayerName=top.opener.screen.controls.getLayerName(index);
			
			if (curLayerName == selectedLayerName || (curLayerName + " [hidden]") == selectedLayerName)
			{
				top.opener.top.screen.controls.showDocumentLayer(curLayerName, true);
			}
			else
			{
				top.opener.top.screen.controls.showDocumentLayer(curLayerName, false);
			}
		}	
		

	}
	else if (document && document.layers && document.layers["layerSwitch"] && document.layers["layerSwitch"].document.forms[0] && document.layers["layerSwitch"].document.layers['g_hideothers'].document.forms[0]["hideLayers"])
		document.layers["layerSwitch"].document.layers['g_hideothers'].document.forms[0]["hideLayers"].checked = false;
		
}


function cfgHelp()
{
	if ((!cfgHelpWindow) || (cfgHelpWindow==null) || (!cfgHelpWindow.location) || (cfgHelpWindow.closed))
	{
		cfgHelpWindow = top.open("about:blank", "CFGHELP", "dependent=yes,alwaysRaised=yes,width=300,height=230,toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no");		
	}	

	setHelpLocation();
}


function setHelpLocation()
{

	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (cfgHelpWindow && !cfgHelpWindow.closed)	//don't do anything if there's no window, but set location if there is
	{
		var filePrefix = "./cfghelp"; //default prefix, will change
		var helpFile = "./cfghelp/default.htm";	//put default help screen here
	
		//figure out the file prefix by checking the toolbar location
		var toolbarLoc = document.location.toString();
		filePrefix = toolbarLoc.substring(0, toolbarLoc.lastIndexOf("/")+1) + "cfghelp/";
	
		if (top.opener.screen.content.document.location)
		{
			helpFile = top.opener.screen.content.document.location.toString();
			var theIdx = helpFile.lastIndexOf("/");
			var theLength = helpFile.length;
			helpFile = "" + helpFile.substring(theIdx+1, theLength);
		}

		//alert("looking for help file: " + helpFile);

		//check if the file we made up exists, if not, revert to the default
		var helpPath = "";

		if (top.opener && top.opener.top.globals)	
		{
			helpPath = "" + top.opener.top.globals.getFolder(self);
			//alert("helppath: " + helpPath);
		}

		var thePlatform = new String(navigator.userAgent);
		var x=thePlatform.indexOf("(")+1;
		var y=thePlatform.indexOf(";",x+1);
		thePlatform=thePlatform.substring(x,y);

		if (thePlatform == "Macintosh")	
		{						// Macintosh support
			helpPath = helpPath + "cfghelp:";
		}
		else	
		{												// Windows support
			helpPath = helpPath + "cfghelp/";
		}

		var lookingForFile = helpFile;
		helpFile = filePrefix + "default.htm";
		
		var theList = top.opener.top.globals.document.setupPlugin.GetFolderContents(helpPath,".htm");
		if (theList != null)	
		{
			for (var i=0; i<theList.length; i++)	
			{
				if (lookingForFile == theList[i])	{
					helpFile = filePrefix + lookingForFile;
					break;
					}
			}
		}

		if (helpFile != null && helpFile != "")	
		{
			cfgHelpWindow=window.open("about:blank","CFGHELP","dependent=yes,alwaysraised=yes,toolbar=0,location=0,directories=0,status=0,menubar=0,scrollbars=1,resizable=1");
			if (cfgHelpWindow)	{
				cfgHelpWindow.focus();
				//alert("helpfile: " + helpFile);
				cfgHelpWindow.location = helpFile;
			}
		}

	}
}


function exitASE()
{
	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	if (confirm("Are you sure you want to quit the Account Setup Editor?") == true)
		top.opener.top.globals.document.setupPlugin.QuitNavigator();
}


