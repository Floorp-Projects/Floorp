/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 */

var toolkit;
var browser;
var dialog;

function onLoad() {
	dialog = new Object;
	dialog.input     = document.getElementById( "dialog.input" );
    dialog.ok        = document.getElementById( "dialog.ok" );
    dialog.test        = document.getElementById( "dialog.test" );
    dialog.testLabel        = document.getElementById( "dialog.testLabel" );
    dialog.cancel    = document.getElementById( "dialog.cancel" );
    dialog.help      = document.getElementById( "dialog.help" );
	dialog.newWindow = document.getElementById( "dialog.newWindow" );

	toolkit = XPAppCoresManager.Find( "toolkitCore" );
	if ( !toolkit ) {
		toolkit = new ToolkitCore();
		toolkit.Init( "toolkitCore" );
	}

	browser = XPAppCoresManager.Find( window.arguments[0] );
	if ( !browser ) {
		dump( "unable to get browser app core\n" );
		toolkit.CloseWindow( window );
        return;
	}

	/* Give input field the focus. */
	dialog.input.focus();
}

function onTyping( key ) {
   // Look for enter key...
   if ( key == 13 ) {
      // If ok button not disabled, go for it.
      if ( !dialog.ok.disabled ) {
         open();
      }
   } else {
      // Check for valid input.
      if ( dialog.input.value == "" ) {
         // No input, disable ok button if enabled.
         if ( !dialog.ok.disabled ) {
            dialog.ok.setAttribute( "disabled", "" );
         }
      } else {
         // Input, enable ok button if disabled.
         if ( dialog.ok.disabled ) {
            dialog.ok.removeAttribute( "disabled" );
         }
      }
   }
}

function open() {
   if ( dialog.ok.disabled ) {
      return;
   }

	var url = dialog.input.value;

	if ( !dialog.newWindow.checked ) {
		/* Load url in opener. */
		browser.loadUrl( url );
	} else {
		/* User wants new window. */
		toolkit.ShowWindowWithArgs( "chrome:/navigator/content/navigator.xul", window.opener, url );
	}

	/* Close dialog. */
	toolkit.CloseWindow( window );
}

function choose() {
	/* Use existing browser "open" logic. */
	browser.openWindow();
	toolkit.CloseWindow( window );
}

function cancel() {
    toolkit.CloseWindow( window );
}

function help() {
    if ( dialog.help.disabled ) {
        return;
    }
    dump( "openLocation::help() not implemented\n" );
}

function strresTest() {
  var Bundle = srGetStrBundle("resource:/res/strres.properties");
  var	ostr1 = Bundle.GetStringFromName("file");
  dump("\n--** JS strBundle GetStringFromName file=" + ostr1 +
      "len=" + ostr1.length + "**--\n");
  var	ostr2 = Bundle.GetStringFromID(123);
  dump("\n--** JS strBundle GetStringFromID 123=" + ostr2 + 
      "len=" + ostr2.length + "**--\n");

  var	ostr3 = Bundle.GetStringFromName("back");
  dump("\n--** JS strBundle GetStringFromName back=" + ostr3 + 
       "len=" + ostr3.length + "**--\n");
  
  var	ostr4 = Bundle.GetStringFromName("eutf8");
  dump("\n--** JS strBundle GetStringFromName eutf8=" + ostr4 +
       "len=" + ostr4.length + "**--\n");
  var	ostr5 = "\u88e6\ue3bb\u8b82"; /* æˆ»ã‚‹ */
  dump("\n--** JS strBundle GetStringFromName escaped=" + ostr5 +
       "len=" + ostr5.length + "**--\n");


  /* utf-8 \u9cdf\u9b5a */
  dialog.test.setAttribute("value", ostr3); 
  dialog.ok.setAttribute("value", ostr2); 
  dialog.cancel.setAttribute("value", ostr5); 
  /* sjis 
  dialog.test.setAttribute("value", "–ß‚é");  */
}
