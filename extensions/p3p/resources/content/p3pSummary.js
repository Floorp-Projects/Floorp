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

const nsIP3PService    = Components.interfaces.nsIP3PService;
const nsIDocShell      = Components.interfaces.nsIDocShell;
const nsIPromptService = Components.interfaces.nsIPromptService;

var gBrowser       = null;
var gDocument      = null;

function renderMachineReadable()
{
  var xsltp     = window.arguments[0];
  var source    = window.arguments[1];
  var style     = window.arguments[2];
  var policyuri = window.arguments[3];
  var result    = getBrowser().contentDocument;

  try {
    var docshell  = getBrowser().docShell.QueryInterface(nsIDocShell);
    var service   = Components.classes["@mozilla.org/p3p/p3pservice;1"].getService(nsIP3PService);
    
    // For browser security do not allow javascript on the transformed document.
    docshell.allowJavascript = false; 
    
    // Set the policy url on the result document ( fabricated ) where
    // the transformation would result. Also, set the policy uri on the
    // docshell for named anchors to work correctly.
    docshell.setCurrentURI(policyuri);
    service.setDocumentURL(result, policyuri);
    
    xsltp.transformDocument(source, style, result, null);
    window.title = result.getElementById("topic").firstChild.nodeValue;
  }
  catch (ex) {
    alertMessage(getBundle().GetStringFromName("InternalError"));
  }
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function getDocument()
{
  if (!gDocument)
    gDocument = getBrowser().contentDocument;
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
    fp.defaultString = window.title;
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
  if ("parentNode" in aEvent.target && 
      aEvent.target.parentNode.hasAttribute("message")) {
    alertMessage(aEvent.target.parentNode.getAttribute("message"));
  }
}

function alertMessage(aMessage)
{
  if (!gPromptService) {
    gPromptService = 
      Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPromptService);
  }
  gPromptService.alert(window, getBrandName(), aMessage);
}
