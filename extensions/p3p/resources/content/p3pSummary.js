/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

const nsIDocShell      = Components.interfaces.nsIDocShell;

var gBrowser       = null;
var gDocument      = null;

function renderMachineReadable()
{
  var xsltp     = window.arguments[0];
  var source    = window.arguments[1];
  var style     = window.arguments[2];
  var policyuri = window.arguments[3].clone().QueryInterface(nsIURL);
  policyuri.ref = "";

  try {
    var docshell  = getSummaryBrowser().docShell.QueryInterface(nsIDocShell);
    
    // For browser security do not allow javascript on the transformed document.
    docshell.allowJavascript = false; 
    
    // Set the policy url on the result document ( fabricated ) where
    // the transformation would result. Also, set the policy uri on the
    // docshell for named anchors to work correctly.
    var resultDocument = getDocument();
    docshell.setCurrentURI(policyuri);

    xsltp.reset();
    xsltp.setParameter("", "policyUri", policyuri.spec);
    xsltp.importStylesheet(style);

    var result = xsltp.transformToFragment(source, resultDocument);

    // XXX Replacing the documentElement makes scrollbars disappear.
    //     See http://bugzilla.mozilla.org/show_bug.cgi?id=205725.
    //while (resultDocument.lastChild} {
    //    resultDocument.removeChild(resultDocument.lastChild);
    //}
    // XXX appendChild of a DocumentFragment should work but doesn't.
    //     See http://bugzilla.mozilla.org/show_bug.cgi?id=209027.
    //resultDocument.appendChild(result);
    transferToDocument(result, resultDocument);

    document.title = resultDocument.getElementById("topic").firstChild.nodeValue;
  }
  catch (ex) {
    alertMessage(getBundle().GetStringFromName("InternalError"));
  }
}

// XXX Temporary function to workaround disappearing scrollbars
//     and bug in document.appendChild(DocumentFragment).
//     See http://bugzilla.mozilla.org/show_bug.cgi?id=205725
//     and http://bugzilla.mozilla.org/show_bug.cgi?id=209027.
function transferToDocument(aResult, aResultDocument)
{
    var docElement = aResultDocument.documentElement;
    while (aResultDocument.firstChild &&
           aResultDocument.firstChild != docElement) {
        aResultDocument.removeChild(aResultDocument.firstChild);
    }
    while (docElement.lastChild) {
        docElement.removeChild(docElement.lastChild);
    }
    while (aResultDocument.lastChild &&
           aResultDocument.lastChild != docElement) {
        aResultDocument.removeChild(aResultDocument.lastChild);
    }
    var pastDocumentElement = false;
    for (var i = 0; i < aResult.childNodes.length; ++i) {
        var childNode = aResult.childNodes.item(i);
        if (childNode instanceof HTMLElement) {
            while (childNode.firstChild) {
                aResultDocument.documentElement.appendChild(childNode.removeChild(childNode.firstChild));
            }
            pastDocumentElement = true;
        }
        else {
            if (pastDocumentElement) {
                aResultDocument.appendChild(childNode);
            }
            else {
                aResultDocument.insertBefore(childNode, aResultDocument.documentElement);
            }
            // We removed the node from the list, which will cause it
            // to shrink by one element.  "i" is now pointing to the
            // node that used to be _after_ the node we just removed.
            // Decrement i, so when it increments (at the start of the
            // next iteration) we'll point at the right place.
            --i;
        }
    }
}

function getSummaryBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function getDocument()
{
  if (!gDocument)
    gDocument = getSummaryBrowser().contentDocument;
  return gDocument;
}

function p3pSummarySavePage()
{
  try
  {
    // make user pick target file
    const kIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].
      createInstance(kIFilePicker);

    fp.init(window, getBundle().GetStringFromName("savePolicy.title"), 
      kIFilePicker.modeSave);
    fp.appendFilters(kIFilePicker.filterAll | kIFilePicker.filterHTML);
    fp.defaultString = document.title;
    fp.defaultExtension = "html";

    var rv = fp.show();

    // write ``pretty printed'' source to file
    if (rv == kIFilePicker.returnOK || rv == kIFilePicker.returnReplace)
    {
      const kIPersist = Components.interfaces.nsIWebBrowserPersist;
      var persistor = Components.
        classes["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
        createInstance(kIPersist);

        persistor.saveDocument(getDocument(), fp.file, null, "text/html", 
          kIPersist.ENCODE_FLAGS_FORMATTED | 
          kIPersist.ENCODE_FLAGS_ENCODE_BASIC_ENTITIES, 0);
    }
  }
  catch (ex)
  {
    try 
    {
      alertMessage(getBundle().GetStringFromName("saveError"));
    }
    catch (ex)
    {
    }
  }
}

/* This method displays additional information, in an alert box, upon request.
 * Note that since javascript is disabled, for security reasons, in the policy viewer
 * we have to capture the specific event, that bubbles up to the chrome, containing
 * the requested message.
 */
function captureContentClick(aEvent)
{
  if (aEvent.target.hasAttribute("message")) {
    alertMessage(aEvent.target.getAttribute("message"));
    return true;
  }

  return contentAreaClick(aEvent);
}
