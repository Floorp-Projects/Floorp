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



// globals
var theToolBar = null; 					// a global pointer to our toolbar - should it exist
var helpWindow = null;
var preLoaded = false;



compromisePrincipals();



function preLoadImages()
{
	if (preLoaded == false)	{
		//img[0] is the normal image
		//img[1] is the mouseover
		//img[2] is the mousedown
		 
		backImages		= new Array;
		helpImages		= new Array;
		exitImages		= new Array;
		nextImages		= new Array;
		connectImages	= new Array;
		doneImages		= new Array;
		ffImages		= new Array;
		rebootImages	= new Array;

		backImages[0] = new Image(32,32);
		backImages[0].src = "images/bk_up.gif";
		backImages[1] = new Image(32,32);
		backImages[1].src = "images/bk_mo.gif";
		backImages[2] = new Image(32,32);
		backImages[2].src = "images/bk_down.gif";

		helpImages[0] = new Image(32,32);
		helpImages[0].src = "images/hlp_up.gif";
		helpImages[1] = new Image(32,32);
		helpImages[1].src = "images/hlp_mo.gif";
		helpImages[2] = new Image(32,32);
		helpImages[2].src = "images/hlp_down.gif";

		exitImages[0] = new Image(32,32);
		exitImages[0].src = "images/ext_up.gif";
		exitImages[1] = new Image(32,32);
		exitImages[1].src = "images/ext_mo.gif";
		exitImages[2] = new Image(32,32);
		exitImages[2].src = "images/ext_down.gif";

		nextImages[0] = new Image(32,32);
		nextImages[0].src = "images/nxt_up.gif";
		nextImages[1] = new Image(32,32);
		nextImages[1].src = "images/nxt_mo.gif";
		nextImages[2] = new Image(32,32);
		nextImages[2].src = "images/nxt_down.gif";

		rebootImages[0] = new Image(32,32);
		rebootImages[0].src = "images/rb_up.gif";
		rebootImages[1] = new Image(32,32);
		rebootImages[1].src = "images/rb_mo.gif";
		rebootImages[2] = new Image(32,32);
		rebootImages[2].src = "images/rb_down.gif";

		connectImages[0] = new Image(32,32);
		connectImages[0].src = "images/cn_up.gif";
		connectImages[1] = new Image(32,32);
		connectImages[1].src = "images/cn_mo.gif";
		connectImages[2] = new Image(32,32);
		connectImages[2].src = "images/cn_down.gif";

		doneImages[0] = new Image(32,32);
		doneImages[0].src = "images/dn_up.gif";
		doneImages[1] = new Image(32,32);
		doneImages[1].src = "images/dn_mo.gif";
		doneImages[2] = new Image(32,32);
		doneImages[2].src = "images/dn_down.gif";

		ffImages[0] = new Image(32,32);
		ffImages[0].src = "images/ff_up.gif";
		ffImages[1] = new Image(32,32);
		ffImages[1].src = "images/ff_mo.gif";
		ffImages[2] = new Image(32,32);
		ffImages[2].src = "images/ff_down.gif";

		preLoaded = true;
		}
}



function generateControls()
{
	var	editMode				=	false;
	var	showAcctsetEdit			=	false;
	var	showRegFileEdit			=	false;
	var	showISPFileEdit			=	false;
	var showExit				=	true;
	var	showHelp				=	true;
	var	showBack				=	true;
	var	showNext				=	true;
	var	showConnectServer		=	false;
	var	showConnectNow			=	false;
	var showDownload			=	false;
	var	showConnectLater		=	false;
	var	showAgain				=	false;
	var	showDone				=	false;
	var	showRestart				=	false;
	var showSetupShortcut		=	false;
	var	showInternet			=	false;
	var	showScreenToggle		=	false;
	var	screenVisible			=	true;
	var showScreenOptions		=	false;

	netscape.security.PrivilegeManager.enablePrivilege( "AccountSetup" );

	if ( parent && parent.parent && parent.parent.globals )
	{
		editMode = ( parent.parent.globals.document.vars.editMode.value.toLowerCase() == "yes" ) ? true : false;
	}

//	var formName = parent.content.location.toString();
	var formName = "" + parent.content.location;
	if ( formName != null && formName != "" && formName != "about:blank" )
	{
		if ( ( x = formName.lastIndexOf( "/" ) ) > 0 )
		{
			formName = formName.substring( x + 1, formName.length );
		}

		if ( editMode == true )
		{
			var section = null;
			var variable = null;
			var pageNum = findPageOffset( formName );
			if ( pageNum >= 0 )
			{
				section = pages[ pageNum ][ 0 ].section;
				variable = pages[ pageNum ][ 0 ].variable;
				if ( section!=null && section!="" && variable!=null && variable!="" )
				{
					showScreenToggle = true;
					var theFile = parent.parent.globals.getAcctSetupFilename( self );
					var theFlag = parent.parent.globals.GetNameValuePair( theFile, section, variable );
					theFlag = theFlag.toLowerCase();
					if ( theFlag == "no" )
						screenVisible = false;
				}
			}
		}

		if ( formName == "main.htm" )
		{
			showBack = false;
			showNext = false;
			if ( navigator.javaEnabled() == false )
			{
				showNext = false;
				editMode = false;
				showAcctsetEdit = false;
				showISPFileEdit = false;
				showRegFileEdit = false;
				document.writeln( "<CENTER><STRONG>Java support is disabled!<P>\n" );
				document.writeln( "Choose Options | Network Preferences and enable Java, then try again.</STRONG></CENTER>\n" );
			}
			else if ( !navigator.mimeTypes[ "application/x-netscape-autoconfigure-dialer" ] ) 
			{
				showNext = false;
				editMode = false;
				showAcctsetEdit = false;
				showISPFileEdit = false;
				showRegFileEdit = false;
				document.writeln( "<CENTER><STRONG>The 'Account Setup Plugin' is not installed!<P>\n" );
				document.writeln( "Please install the plugin, then run 'Account Setup' again.</STRONG></CENTER>\n" );
			}
			else if ( parent.parent.globals.document.setupPlugin == null )
			{
				showNext = false;
				editMode = false;
			}
			if ( editMode == true )
			{
				showAcctsetEdit = true;
				showScreenOptions = true;
			}
		}
		else if ( editMode == true && formName == "useAcct.htm" )
		{
			showScreenOptions = true;
		} 
		else if ( editMode == true && formName == "servers.htm" )
		{
			showScreenOptions = false;
		}
		else if ( editMode == true && formName == "billing.htm" )
		{
			showScreenOptions=true;
		}
		else if ( formName == "accounts.htm" )
		{
			showNext = false;
		}
		else if ( formName == "connect1.htm" )
		{
			showNext = false;
			showConnectServer = true;
			if ( editMode == true )
				showScreenOptions = true;
		}
		else if ( formName == "download.htm" )
		{
			showNext = false;
			showConnectServer = true;
			if ( editMode == true )
				showScreenOptions = true;
		}
		else if ( formName == "connect2.htm" )
		{
			showNext = false;
			showExit = false;
			showConnectNow = true;
			showConnectLater = true;
		}
		else if ( formName == "1step.htm" )
		{
			showNext = false;
			showExit = false;
			showHelp = false;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "register.htm" )
		{
			showHelp = false;
			showBack = false;
			showNext = false;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "ok.htm" )
		{
			showScreenOptions = true;
			showBack = false;
			showExit = false;
			showNext = false;
			showInternet = true;
			showDone = true;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "okreboot.htm" )
		{
			showScreenOptions = true;
			showBack = false;
			showNext = false;
			showExit = false;
			showDone = false;
			showRestart = true;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "error.htm" )
		{
			showBack = true;
			showExit = true;
			showNext = false;
			showAgain = true;
			showDone = false;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "later.htm" )
		{
			showBack = false;
			showExit = false;
			showNext = false;
			showDone = true;
			if ( editMode == true )
				showBack = true;
		}
		else if ( formName == "intro1.htm" )
		{
			showSetupShortcut = true;
		}
		else if ( formName == "settings.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "editregs.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "editisps.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "aboutbox.htm" )
		{
			showHelp = false;
			showNext = false;
			showBack = true;
		}
		else if ( formName == "namepw.htm" )
		{
			showScreenOptions = true;
		}
		else if ( formName == "asktty.htm" )
		{
			showScreenOptions = false;
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "askserv.htm" )
		{
			showScreenOptions = false;
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "asksvinf.htm" )
		{
			showScreenOptions = false;
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "showphon.htm" )
		{
			showScreenOptions = false;
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "editcc.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "addnci.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "addias.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}
		else if ( formName == "editfour.htm" )
		{
			showBack = true;
			showNext = false;
			editMode = false;
		}

		if ( document && document.layers && document.layers[ "controls" ] && document.layers[ "controls" ].document && document.layers[ "controls" ].document.layers && document.layers[ "controls" ].document.layers.length > 0 )
		{
			document.layers[ "controls" ].layers[ "help" ].visibility = ( ( showHelp == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "exit" ].visibility = ( ( showExit == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "back" ].visibility = ( ( showBack == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "next" ].visibility = ( ( showNext == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "connectnow" ].visibility = ( ( showConnectNow == true ) ? "show" : "hide" );
			document.layers[ "controls" ].layers[ "download" ].visibility = ( ( showDownload == true ) ? "show" : "hide" ); 
			document.layers[ "controls" ].layers[ "connectserver" ].visibility = ( ( showConnectServer == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "connectagain" ].visibility = ( ( showAgain == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "done" ].visibility = ( ( showDone == true) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "restart" ].visibility = ( ( showRestart == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "connectlater" ].visibility = ( ( showConnectLater == true ) ? "show" : "hide" );			
			document.layers[ "controls" ].layers[ "setup" ].visibility = ( ( showSetupShortcut == true ) ? "show" : "hide" );			

			//NEW - Generate the controls for the toolbar, if it exists
			if ( ( !theToolBar ) || ( theToolBar == null ) || ( !theToolBar.location ) || ( theToolBar.closed ) )
			{
				//alert("opening toolbar");
				theToolBar = openToolBar();
			}
			else
			{
				//alert("toolbar open, generating controls" + theToolBar);
				generateToolBarControls();	
			}
		}
		else
		{
			setTimeout( "generateControls()", 1000 );
		}
		

		}
	else
	{
		setTimeout( "generateControls()", 1000 );
	}
}



function doHelp(formName)
{
		var thePlatform;

	netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

	helpFile = "./help/ashelp.htm";

	var helpPath = "";
	if (parent && parent.parent && parent.parent.globals)	{
		helpPath = "" + parent.parent.globals.getFolder(self);

		thePlatform = new String(navigator.userAgent);
		var x=thePlatform.indexOf("(")+1;
		var y=thePlatform.indexOf(";",x+1);
		thePlatform=thePlatform.substring(x,y);
		if (thePlatform == "Macintosh")	{						// Macintosh support
			helpPath = helpPath + "help:";
			}
		else	{												// Windows support
			helpPath = helpPath + "help/";
			}

		if (thePlatform != "Macintosh") {

			var hpath=unescape(location.pathname);
			hpath = hpath.substring(0, hpath.lastIndexOf('/'));
			helpFile = hpath + "/help/ashelp.htm";

			// get rid of the return char at the end of .htm
			formName = formName.substring(0, formName.indexOf('.htm')+4);
			// next, get rid of sub folders in formName
			while (formName.indexOf('/') > 0)
				formName = formName.substring(formName.indexOf('/')+1, formName.length);
		}

		var theList = parent.parent.globals.document.setupPlugin.GetFolderContents(helpPath,".htm");
		if (theList != null)	{
			for (var i=0; i<theList.length; i++)	{
				if (formName == theList[i])	{
					if (thePlatform != "Macintosh") {
						var currentpath=unescape(location.pathname);
						currentpath = currentpath.substring(0, currentpath.lastIndexOf('/'));
						helpFile = currentpath + "/help/" + formName;
					} else { 
						helpFile = "./help/" + formName;
					}
					break;
					}
				}
			}
		}

	if (helpFile != null && helpFile != "")	{
		if (helpWindow == null || helpWindow.closed)	{
			helpWindow=window.open("about:blank","Documentation","width=328,height=328,alwaysRaised=yes,dependent=yes,toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=yes,resizable=yes");
			}
		if (helpWindow && helpWindow != null)	{
			helpWindow.focus();
			helpWindow.location = helpFile;
			}
		}
}



//here are a bunch of functions for the floating toolbar



function openToolBar()
{

		var thePlatform = new String(navigator.userAgent);
		var x=thePlatform.indexOf("(")+1;
		var y=thePlatform.indexOf(";",x+1);
		thePlatform=thePlatform.substring(x,y);

		var	editMode = false;

	if (parent && parent.parent && parent.parent.globals)	{
		editMode=(parent.parent.globals.document.vars.editMode.value.toLowerCase() == "yes") ? true:false;
		}

	if (editMode == true)	
	{
		if (!(theToolBar) || (theToolBar == null) || !(theToolBar.location)) 
		{
			if (thePlatform == "Macintosh")	
				theToolBar = top.open("../../Tools/Kit/config.htm","Configurator","width=400,height=104,dependent=yes,alwaysraised=yes,toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no");
			else
				theToolBar = top.open("../../../AccountSetupTools/Kit/config.htm","Configurator","width=400,height=104,dependent=yes,alwaysraised=yes,toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no");
				
		}
	}
	else
	{
		theToolBar = null;
	}
	return theToolBar;
}



function showLayer(layerName, showIfTrue)
{	
	//alert("showLayer "+showIfTrue);
	//parent.parent.globals.debug("showing layer: " + layerName + " " + showIfTrue + " layers: " + theToolBar.document.layers.length + " " + theToolBar.document.layers[layerName]);
	if ((theToolBar) && (theToolBar!=null)  && (theToolBar.location) && (theToolBar.finishedLoading()))
	{
				var gLayerName = "g_" + layerName;
				//var theLayer = eval("theToolBar.document.layers." + layerName);
				//parent.parent.globals.debug("theLayer: "+theLayer+ " but t.d.l.l: " + theToolBar.document.layers[layerName]);
				//var gLayer = eval("theToolBar.document.layers.g_" + layerName);
				
				//if (theLayer)
				{
					if (showIfTrue == true)
					{
						//theToolBar.document.layers[layerName].visibility ="show";
						theToolBar.showlayer(layerName);
						theToolBar.hidelayer(gLayerName);
						//theLayer.visibility="show";
						//if (gLayer) gLayer.visibility="hide";
					}
					else
					{
						theToolBar.hidelayer(layerName);
						theToolBar.showlayer(gLayerName);
						//theToolBar.document.layers[layerName].visibility ="hide";	
						//theLayer.visibility="hide";
						//if (gLayer) gLayer.visibility="show";
					}
				}
				//else
					//theToolBar.history.go(0);

	}
}

function screenVisible()
{
	var isVisible = true;
	var formName = parent.content.location.toString();
	if (formName!=null && formName!="")	{
		if ((x=formName.lastIndexOf("/"))>0)	{
			formName=formName.substring(x+1,formName.length);
			}
			var section=null;
			var variable=null;
			var pageNum=findPageOffset(formName);
			if (pageNum>=0)	{
				section=pages[pageNum][0].section;
				variable=pages[pageNum][0].variable;
				if (section!=null && section!="" && variable!=null && variable!="")	{
					var theFile = parent.parent.globals.getAcctSetupFilename(self);
					var theFlag = parent.parent.globals.GetNameValuePair(theFile,section, variable);
					theFlag = theFlag.toLowerCase();
					if (theFlag == "no")	
						isVisible=false;	
					}
				}
	//alert("clayer: screenVisible = : " + isVisible + "flag (" + variable + ") = " + theFlag);
	}
	return isVisible;
}

function showScreen(inValue)
{
	//alert("in showscreen");
	var formName = parent.content.location.toString();
	
	if (formName!=null && formName!="")	
	{
		if ((x=formName.lastIndexOf("/"))>0)	{
		formName=formName.substring(x+1,formName.length);
		}

		//alert("formName: " + formName);
		var pageNum=findPageOffset(formName);
		if (pageNum>=0)	
		{
			var section=pages[pageNum][0].section;
			var variable=pages[pageNum][0].variable;
			//alert("sec: " + section + " var: "+variable); 
			if (section!=null && section!="" && variable!=null && variable!="")	{
				var theFile = parent.parent.globals.getAcctSetupFilename(self);
				var theFlag; // = parent.parent.globals.GetNameValuePair(theFile,section, variable);


				//theFlag = theFlag.toLowerCase();
				//if (theFlag == "no")	theFlag="yes";
				//else			theFlag="no";
				
				if (inValue == false)
					theFlag = "no";
				else
					theFlag = "yes";
				
				//alert("Setting flag " + variable + " to " + theFlag + " invalue: " + inValue);
				parent.parent.globals.SetNameValuePair(theFile,section, variable,theFlag);
				return (theFlag == "yes");
				}
		}		
		else
		{
			alert("This screen cannot be suppressed");
			return true;	//forces the checkbox back on
		}
	}
}


function generateToolBarControls()
{
	var editMode				=	false;
//	var	editMode				=	parent.parent.editMode();
	var	showScreenToggle		=	false;	
	var showScreenOptions		=	false;
	var isScreenVisible			=	true;
	var showEditHelp			= 	true;
	
	//var	showAcctsetEdit			=	false;
	//var	showRegFileEdit			=	false;
	//var	showISPFileEdit			=	false;

	if (parent && parent.parent && parent.parent.globals)	{
		editMode=(parent.parent.globals.document.vars.editMode.value.toLowerCase() == "yes") ? true:false;
		}

//	var formName = parent.content.location.toString();
	var formName = "" + parent.content.location;

	if (formName!=null && formName!="")	
	{
		if ((x=formName.lastIndexOf("/"))>0)	{
			formName=formName.substring(x+1,formName.length);
			}

		// this decides whether we should show the checkbox
		var pageNum=findPageOffset(formName);
		if (pageNum>=0)	
		{
			section=pages[pageNum][0].section;
			variable=pages[pageNum][0].variable;
			if (section!=null && section!="" && variable!=null && variable!="")	{
				showScreenToggle=true;
				//var theFile = parent.parent.globals.getAcctSetupFilename(self);
				//var theFlag = parent.parent.globals.GetNameValuePair(theFile,section, variable);
				//theFlag = theFlag.toLowerCase();
				//if (theFlag == "no")	screenVisible=false;
				}
		}


		if (formName == "main.htm")	{
			showScreenOptions=true;
			}
		else if (formName == "useAcct.htm")	{
			showScreenOptions=true;
			}
		else if (formName == "servers.htm")	{
			showScreenOptions=false;
			}
		else if (formName == "billing.htm")	{
			showScreenOptions=true;
			}
		else if (formName == "connect1.htm")	{
				showScreenOptions=true;
			}
		else if (formName == "connect2.htm")	{
			}
		else if (formName == "register.htm")	{
			}
		else if (formName == "ok.htm")	{
			showScreenOptions=true;
			}
		else if (formName == "okreboot.htm")	{
			showScreenOptions=true;
			}
		else if (formName == "error.htm")	{
			}
		else if (formName == "later.htm")	{
			}
		else if (formName == "settings.htm")	{
			editMode = false;
			}
		else if (formName == "editregs.htm")	{
			editMode=false;
			}
		else if (formName == "editisps.htm")	{
			editMode=false;
			}
		else if (formName == "aboutbox.htm")	{
			}
		else if (formName == "namepw.htm")		{
			showScreenOptions=true;
			}
		else if ((formName == "asktty.htm")) 	{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "askserv.htm")		{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "asksvinf.htm")	{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "showphon.htm")	{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "editcc.htm")	{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "addnci.htm")		{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "addias.htm")		{
			showScreenOptions=false;
			editMode=false;
			}
		else if (formName == "editfour.htm")		{
			showScreenOptions=false;
			editMode=false;
			}


		if (theToolBar && theToolBar != null && (theToolBar.location) && (theToolBar.document.layers) && !(theToolBar.closed)
			&& (theToolBar.ready) && (theToolBar.ready == true))
		{

			if (theToolBar.finishedLoading())
			{
				showLayer("reload", editMode);
				showLayer("edit", editMode);
				showLayer("chooseed", editMode);
				showLayer("edithelp", editMode);
				showLayer("options", showScreenOptions);
				showLayer("showscreen", showScreenToggle);
				if (showScreenToggle == true)
					theToolBar.setShowScreenBox();
				
				theToolBar.updateLayersLayer();
				//alert("found toolbar! - setting help location");
				theToolBar.setHelpLocation();
				
			}
			else
			{
				//alert("Found toolbar, without reloadlayer!");
				theToolBar.history.go(0);	
			}
		}
		//else
			//alert("warning: toolbar not found");
	}
}

function showDocumentLayer(inLayerName, inDoShow)
{
	if (inDoShow == "hide")
		inDoShow = false;
	else if (inDoShow != false)
		inDoShow = true;
	
	if (parent.content && parent.content.document.layers[inLayerName])
	{
		if (inDoShow == true)
		{
			parent.content.document.layers[inLayerName].visibility = "show";
			//alert("showing layer: " + inLayerName);
		}
		else
		{
			parent.content.document.layers[inLayerName].visibility = "hide";
			//alert("hiding layer: " + inLayerName);
		}
	}

}


function countDocumentLayers()
{
	if (parent.content.document.layers)
		return parent.content.document.layers.length;
	else
		return 0;
}

function getLayerName(inIndex)
{
	if (parent.content.document.layers && parent.content.document.layers[inIndex])
		return parent.content.document.layers[inIndex].name;
	return null;
}

function getLayerSrc(inIndex)
{
	if (parent.content.document.layers && parent.content.document.layers[inIndex])
		return parent.content.document.layers[inIndex].src;
	return null;
}

function getDocumentLocation()
{
	return parent.content.document.location;
}

function getDocumentLayerVisibility(inLayerName)
{
	if (parent.content.document.layers && parent.content.document.layers[inLayerName])
		return parent.content.document.layers[inLayerName].visibility;
	else 
		return null;

}

function reloadDocument()
{
	top.globals.debug("RELOADING DOCUMENT!!!!" + parent.content.history);
	if (parent.content.history)
		parent.content.history.go(0);
}
// end hiding contents from old browsers  -->
