/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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
 *
 * Contributor(s):
 */
 
 var gIsEncrypted = -1;
 var gWalletService = -1;

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
      var prefillState = getState(prefill, 3);
      showItem("formToolbar", (prefillState != hide));

      // enable prefill button if there is at least one saved value for the form
      setDisabledAttr("formPrefill", (prefillState == disable));
    }
*/

    function formShow() {
      window.openDialog(
          "chrome://communicator/content/wallet/WalletViewer.xul",
          "_blank",
          "chrome,titlebar,modal=yes,resizable=yes");
/* form toolbar is out
       initToolbarItems(); // need to redetermine which buttons in form toolbar to enable
*/
    }

    // Capture the values that are filled in on the form being displayed.
    function formCapture() {
      var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
      walletService.WALLET_RequestToCapture(window._content);
    }

    // Prefill the form being displayed.
    function formPrefill() {
      var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
      try {
        walletService.WALLET_Prefill(false, window._content);
        window.openDialog("chrome://communicator/content/wallet/WalletPreview.xul",
                          "_blank", "chrome,modal=yes,dialog=yes,all, width=504, height=436");
      } catch(e) {
      }
    }

/*
    // Prefill the form being displayed without bringing up the preview window.
    function formQuickPrefill() {
      gWalletService.WALLET_Prefill(true, window._content);
    }
*/

    var hide = -1;
    var disable = 0;
    var enable = 1;

    var capture = 0;
    var prefill = 1;

    var elementCount;

    // Walk through the DOM to determine how a capture or prefill item is to appear.
    //   arguments:
    //      captureOrPrefill = capture, prefill
    //   returned value:
    //      hide, disable, enable
    function getStateFromFormsArray(formsArray, captureOrPrefill, threshhold) {
      if (!formsArray) {
        return hide;
      }

      var form;
      var bestState = hide;

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
                gIsEncrypted = this.pref.GetBoolPref("wallet.crypto");
              if (gIsEncrypted) {
                // database is encrypted, see if it is still locked
//              if (locked) { -- there's currently no way to make such a test
                  // it's encrypted and locked, we lose
                  elementCount = threshhold+1;
                  return enable;
//              }
              }
            } catch(e) {
              // there is no crypto pref so database could not possible be encrypted
            }
            
            if (bestState == hide) {
              bestState = disable;
            }
            var value;

            // obtain saved values if any and store in array called valueList
            var valueList;
            var valueSequence = gWalletService.WALLET_PrefillOneElement
              (window._content, elementsArray[element]);
            // result is a linear sequence of values, each preceded by a separator character
            // convert linear sequence of values into an array of values
            if (valueSequence) {
              var separator = valueSequence[0];
              valueList = valueSequence.substring(1, valueSequence.length).split(separator);
            }

            // see if there's a value on screen (capture case) or a saved value (prefill case) 
            if (captureOrPrefill == capture) {
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
            } else {
              // in prefill case, see if element has a saved value
              if (valueSequence) {
                value = valueList[0];
              }
            }

            if (value) {
              // at least one text (or select) element has a value,
              //    in which case the capture or prefill item is to appear in menu
              bestState = enable;
              if (elementCount > threshhold) {
                return enable;
              }
            }
          } 
        }
      }
      // if we got here, then there was no element with a value or too few elements
      return bestState;
    }

    var bestState;

    function stateFoundInFormsArray(formsArray, captureOrPrefill, threshhold) {
      var state =
        getStateFromFormsArray(formsArray, captureOrPrefill, threshhold);
      if (state == enable) {
        if (elementCount > threshhold) {
          bestState = enable;
          return true;
        }
        bestState = enable;
      } else if (state == disable && bestState == hide) {
        bestState = disable;
        return false;
      }
    }

    // Walk through the DOM to determine how capture or prefill item is to appear.
    //   arguments:
    //      captureOrPrefill = capture, prefill
    //   returned value:
    //      hide, disable, enable

    function getState(captureOrPrefill, threshhold) {
      stateFound(window.content, captureOrPrefill, threshhold);
      return bestState;
    }

    function stateFound(content, captureOrPrefill, threshhold) {
      bestState = hide;
      if (!content || !content.document) {
        return false;
      }
      var document = content.document;
      if (!("forms" in document)) {
        // this will occur if document is xul document instead of html document for example
        return false;
      }

      // test for wallet service being available
      if (gWalletService == -1)
        gWalletService = Components.classes["@mozilla.org/wallet/wallet-service;1"]
                                   .getService(Components.interfaces.nsIWalletService);
      if (!gWalletService) {
        return true;
      }

      var state;
      elementCount = 0;

      // process frames if any
      var formsArray;
      var framesArray = content.frames;
      if (framesArray.length != 0) {
        var frame;
        for (frame=0; frame<framesArray.length; ++frame) {

          // recursively process each frame for additional documents
          if (stateFound(framesArray[frame], captureOrPrefill, threshhold)) {
            return true;
          }

          // process the document of this frame
          var frameDocument = framesArray[frame].document;
          if (frameDocument) {

            if (stateFoundInFormsArray(frameDocument.forms, captureOrPrefill, threshhold)) {
              gIsEncrypted = -1;
              return true;
            }
          }
        }
      }

      // process top-level document
      gIsEncrypted = -1;
      if (stateFoundInFormsArray(document.forms, captureOrPrefill, threshhold)) {
        return true;
      }

      // if we got here, then there was no text (or select) element with a value
      // or there were too few text (or select) elements
      if (elementCount > threshhold) {
        // no text (or select) element with a value
        return false;
      }

      // too few text (or select) elements
      bestState = hide;
      return false;
    }
