/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

	browser = XPAppCoresManager.Find( window.arguments[0] );
	if ( !browser ) {
		dump( "unable to get browser app core\n" );
        window.close();
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
        window.openDialog( "chrome:/navigator/content/", "_blank", "chrome,dialog=no,all", url );
	}

	/* Close dialog. */
    window.close();
}

function choose() {
	/* Use existing browser "open" logic. */
	browser.openWindow();
    window.close();
}

function cancel() {
    window.close();
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

  var	ostr3 = Bundle.GetStringFromName("loyal");
  dump("\n--** JS strBundle GetStringFromName loyal=" + ostr3 + 
       "len=" + ostr3.length + "**--\n");
  
  var	ostr4 = Bundle.GetStringFromName("trout");
  dump("\n--** JS strBundle GetStringFromName eutf8=" + ostr4 +
       "len=" + ostr4.length + "**--\n");
  var	ostr5 = "\u88e6\ue3bb\u8b82"; /* 戻る */
  dump("\n--** JS strBundle GetStringFromName escaped=" + ostr5 +
       "len=" + ostr5.length + "**--\n");


  /* utf-8 \u9cdf\u9b5a */
  dialog.ok.setAttribute("value", ostr2); 
  dialog.test.setAttribute("value", ostr3); 
  dialog.cancel.setAttribute("value", ostr4); 
}
