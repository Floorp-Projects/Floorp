/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

    var gIsEncrypted = -1;
    var gWalletService = -1;

    // states that the capture and prefill menu items can be in
    const hide = -1;   // don't show menu item
    const disable = 0; // show menu item but gray it out
    const enable = 1;  // show menu item normally

    // Set the disabled attribute of specified item.
    //   If the value is false, then it removes the attribute
    function setDisabledAttr(id, val) {
      var elem = document.getElementById(id);
      if (elem) {
        if (val) {
          elem.setAttribute("disabled", val);
        } else {
          elem.removeAttribute("disabled");
        }
      }
    }

    // Show/hide one item (specified via name or the item element itself).
    function showItem(itemOrId, show) {
      var item = null;
      if (itemOrId.constructor == String) {
        // Argument specifies item id.
        item = document.getElementById(itemOrId);
      } else {
        // Argument is the item itself.
        item = itemOrId;
      }
      if (item) {
        var styleIn = item.getAttribute("style");
        var styleOut = styleIn;
        if (show) {
          // Remove style="display:none;".
          styleOut = styleOut.replace("display:none;", "");

        } else {
          // Set style="display:none;".
          if (styleOut.indexOf("display:none;") == -1) {
            // Add style the first time we need to.
            styleOut += "display:none;";
          }
        }
        // Only set style if it's different.
        if (styleIn != styleOut) {
          item.setAttribute("style", styleOut);
        }
      }
    }

/* form toolbar is out
    var firstTime = true;

    function initToolbarItems() {

      // This routine determines whether or not to display the form-manager toolbar and,
      // if so, which buttons on the toolbar are to be enabled.  We need to reexecute
      // this routine whenever the form-manager dialog finishes because saved values might
      // have been added/removed which could affect the enable/disable state of the buttons.
 
      if (firstTime) {
        // Force initToolbarItems to be executed upon return from viewing prefs.
        //   This is necessary in case the form-manager dialog was invoked from the
        //   pref panel.  See next block of code for more details.
        var pref = document.getElementById("menu_preferences");
        if (pref) {
          oncommand = pref.getAttribute("oncommand");
          pref.setAttribute("oncommand", oncommand+";initToolbarItems()");
          firstTime = false;
        }
      }

      // get the form-manager toolbar
      var cmd_viewformToolbar = document.getElementById("cmd_viewformtoolbar");
      if (!cmd_viewformToolbar) {
        // This happens when you access the form-manager dialog from the prefs panel
        // Not sure yet how to get access to items in navigator in that case
        // So instead we will execute initToolbarItems when edit->prefs returns (that's
        // what above block of code involving firstTime accomplished.
        return;
      }

      // keep form toolbar hidden if checkbox in view menu so indicates
      var checkValue = cmd_viewformToolbar.getAttribute("checked");
      if (checkValue == "false") {
        showItem("formToolbar", false);
        return;
      }

      // hide form toolbar if three or less text elements in form
      var state = getState(3);
      showItem("formToolbar", (state.prefill != hide));

      // enable prefill button if there is at least one saved value for the form
      setDisabledAttr("formPrefill", (state.prefill == disable));
    }
*/

    function formShow() {
      window.openDialog(
          "chrome://communicator/content/wallet/WalletViewer.xul",
          "_blank",
          "chrome,titlebar,resizable=yes");
/* form toolbar is out
       initToolbarItems(); // need to redetermine which buttons in form toolbar to enable
*/
    }

    // Capture the values that are filled in on the form being displayed.
    function formCapture() {
      var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
      walletService.WALLET_RequestToCapture(window.content);
    }

    // Prefill the form being displayed.
    function formPrefill() {
      var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
      try {
        walletService.WALLET_Prefill(false, window.content);
        window.openDialog("chrome://communicator/content/wallet/WalletPreview.xul",
                          "_blank", "chrome,modal=yes,dialog=yes,all, width=504, height=436");
      } catch(e) {
      }
    }

/*
    // Prefill the form being displayed without bringing up the preview window.
    function formQuickPrefill() {
      gWalletService.WALLET_Prefill(true, window.content);
    }
*/

    var elementCount;

    // Walk through the DOM to determine how a capture or prefill item is to appear.
    //   returned value:
    //      hide, disable, enable for .capture and .prefill
    function getStateFromFormsArray(content, threshhold) {
      var formsArray = content.document.forms;
      if (!formsArray) {
        return {capture: hide, prefill: hide};
      }

      var form;
      var bestState = {capture: hide, prefill: hide};

      for (form=0; form<formsArray.length; form++) {
        var elementsArray = formsArray[form].elements;
        var element;
        for (element=0; element<elementsArray.length; element++) {
          var type = elementsArray[element].type;
          if ((type=="") || (type=="text") || (type=="select-one")) {
            // we have a form with at least one text or select element
            if (type != "select-one") {
              elementCount++;
            }

            /* If database is encrypted and user has not yet supplied master password,
             * we won't be able to access the data.  In that case, enable the item rather
             * than asking user for password at this time.  Otherwise you'll be asking for
             * the password whenever user clicks on edit menu or context menu
             */
            try {
              if (gIsEncrypted == -1)
                gIsEncrypted = this.pref.getBoolPref("wallet.crypto");
              if (gIsEncrypted) {
                // database is encrypted, see if it is still locked
//              if (locked) { -- there's currently no way to make such a test
                  // it's encrypted and locked, we lose
                  elementCount = threshhold+1;
                  return {capture: enable, prefill: enable};
//              }
              }
            } catch(e) {
              // there is no crypto pref so database could not possible be encrypted
            }

            // since we know there is at least one text or select element, we can unhide
            // the menu items.  That means doing nothing if state is already "enable" but
            // changing state to "disable" if it is currently "hide".
            for (var j in bestState) {
              if (bestState[j] == hide) {
                bestState[j] = disable;
              }
            }
            
            var value;

            // obtain saved values if any and store in array called valueList
            var valueList;
            var valueSequence = gWalletService.WALLET_PrefillOneElement
              (content, elementsArray[element]);
            // result is a linear sequence of values, each preceded by a separator character
            // convert linear sequence of values into an array of values
            if (valueSequence) {
              var separator = valueSequence[0];
              valueList = valueSequence.substring(1, valueSequence.length).split(separator);
            }

            // in capture case, see if element has a value on the screen which is not saved
            value = elementsArray[element].value;
            if (valueSequence && value) {
              for (var i=0; i<valueList.length; i++) {
                if (value == valueList[i]) {
                  value = null;
                  break;
                }
              }
            }
            if (value) {
              // at least one text (or select) element has a value,
              //    in which case the capture item is to appear in menu
              bestState.capture = enable;
            }

            // in prefill case, see if element has a saved value
            if (valueSequence) {
              value = valueList[0];
              if (value) {
                // at least one text (or select) element has a value,
                //    in which case the prefill item is to appear in menu
                bestState.prefill = enable;
              }
            }

            // see if we've gone far enough
            if ((bestState.capture == enable) && (bestState.prefill == enable) &&
                (elementCount > threshhold)) {
              return bestState;
            }
          } 
        }
      }
      // if we got here, then there was no element with a value or too few elements
      return bestState;
    }

    var bestState;

    function stateFoundInFormsArray(content, threshhold) {
      var rv = {capture: false, prefill: false};
      var state = getStateFromFormsArray(content, threshhold);
      for (var i in state) {
        if (state[i] == enable) {
          bestState[i] = enable;
          if (elementCount > threshhold) {
            rv[i] = true;
          }
        } else if (state[i] == disable && bestState[i] == hide) {
          bestState[i] = disable;
        }
      }
      return rv;
    }

    // Walk through the DOM to determine how capture or prefill item is to appear.
    //   returned value:
    //      hide, disable, enable for .capture and .prefill

    function getState(threshhold) {
      bestState = {capture: hide, prefill: hide};
      elementCount = 0;
      stateFound(window.content, threshhold);
      return bestState;
    }

    function stateFound(content, threshhold) {
      var captureStateFound = false;
      var prefillStateFound = false;
      if (!content || !content.document) {
        return {capture: false, prefill: false};
      }
      var document = content.document;
      if (!("forms" in document)) {
        // this will occur if document is xul document instead of html document for example
        return {capture: false, prefill: false};
      }

      // test for wallet service being available
      if (gWalletService == -1)
        gWalletService = Components.classes["@mozilla.org/wallet/wallet-service;1"]
                                   .getService(Components.interfaces.nsIWalletService);
      if (!gWalletService) {
        return {capture: true, prefill: true};
      }

      // process frames if any
      var framesArray = content.frames;
      var rv;
      if (framesArray.length != 0) {
        var frame;
        for (frame=0; frame<framesArray.length; ++frame) {

          // recursively process each frame for additional documents
          rv = stateFound(framesArray[frame], threshhold);
          captureStateFound |= rv.capture; prefillStateFound |= rv.prefill;
          if (captureStateFound && prefillStateFound) {
            return {capture: true, prefill: true};
          }

          // process the document of this frame
          var frameContent = framesArray[frame];
          if (frameContent.document) {
            rv = stateFoundInFormsArray(frameContent, threshhold);
            captureStateFound |= rv.capture; prefillStateFound |= rv.prefill;
            if (captureStateFound && prefillStateFound) {
              gIsEncrypted = -1;
              return {capture: true, prefill: true};
            }
          }
        }
      }

      // process top-level document
      gIsEncrypted = -1;
      rv = stateFoundInFormsArray(content, threshhold);
      captureStateFound |= rv.capture; prefillStateFound |= rv.prefill;
      if (captureStateFound && prefillStateFound) {
        return {capture: true, prefill: true};
      }

      // if we got here, then there was no text (or select) element with a value
      // or there were too few text (or select) elements
      if (elementCount > threshhold) {
        // no text (or select) element with a value
        return rv;
      }

      // too few text (or select) elements
      bestState = {capture: hide, prefill: hide};
      return rv;
    }

    // display a Wallet Dialog
    function WalletDialog(which) {
      switch( which ) {
        case "walletsites":
          window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul",
                            "_blank","chrome,resizable","W"); 
          break;
        case "wallet":
        default:
          formShow();
          break;
      }
    }
