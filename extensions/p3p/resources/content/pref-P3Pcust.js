/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

var mybox;
var elementIDs;

function Init() {
  var boxname = window.arguments[0];

  // Find the parent's box and make a copy of it for ourselves.
  var parentBox = window.opener.document.getElementById( boxname );
  if (parentBox) {
    mybox = parentBox.cloneNode( "true" );

    // Put our copy of the box into our window.
    var insertpoint = document.getElementById( "insertboxhere" );
    if (insertpoint) {
      insertpoint.appendChild( mybox );

      // Run the initialization code.
      initCustomize();
    }
  }

  doSetOKCancel( onOK, onCancel );
}

function onOK () {
  // Copy my settings back to the parent window.
  for (var i = 0; i < settingIDs.length; i++) {
    var myElement = document.getElementById( settingIDs[i] );
    if (myElement) {
      var parentElement = window.opener.document.getElementById( settingIDs[i] );
      if (parentElement) {
        parentElement.setAttribute( "checked", myElement.getAttribute( "checked" ) );
      }
    }
  }

  window.close();
}

function onCancel () {
  // Just bail out.
  window.close();
}
