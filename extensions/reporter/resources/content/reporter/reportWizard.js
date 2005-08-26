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
 * The Original Code is Mozilla Reporter (r.m.o).
 *
 * The Initial Developer of the Original Code is
 *      Robert Accettura <robert@accettura.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Boris Zbarsky <bzbarsky@mit.edu>
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

/********************************************************
 *                *** Warning ****
 *   DO _NOT_ MODIFY THIS FILE without first contacting
 *   Robert Accettura <robert@accettura.com>
 *   or a reporter.mozilla.org Admin!
 *******************************************************/

const gURL = window.arguments[0];
const gLanguage = window.navigator.language;
const gRMOvers = "0.2"; // Do not touch without contacting reporter admin!

// Globals
var gReportID;
var gSysID;
var gFaultCode;
var gFaultMessage;
var gSOAPerror = false;
var gPrefBranch;

function getReporterPrefBranch() {
  if (!gPrefBranch) {
    gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService)
                            .getBranch("extensions.reporter.");
  }
  return gPrefBranch;
}

function getBoolPref(prefname, aDefault) {
  try {
    var prefs = getReporterPrefBranch();
    return prefs.getBoolPref(prefname);
  }
  catch(ex) {
    return aDefault;
  }
}

function getCharPref(prefname, aDefault) {
  try {
    var prefs = getReporterPrefBranch();
    return prefs.getCharPref(prefname);
  }
  catch(ex) {
    return aDefault;
  }
}

function initPrivacyNotice() {
  var reportWizard = document.getElementById('reportWizard');
  // If they agreed, we continue on
  if (getBoolPref("hidePrivacyStatement", false)) {
    reportWizard.advance();
  } else {
    // Don't let users rewind or go forward
    reportWizard.canRewind = false;
    reportWizard.canAdvance = false;

    // Load Privacy Policy
    var privacyURL = getCharPref("privacyURL", "http://reporter.mozilla.org/privacy/");
    document.getElementById("privacyStatement").setAttribute("src", privacyURL+"?plain");
  }
}

function privacyPolicyCheckbox() {
  // if checked, hide message and enable forward button, otherwise show the
  // message and disable the forward button.
  var canAdvance = document.getElementById('dontShowPrivacyStatement').checked;
  document.getElementById('reportWizard').canAdvance = canAdvance;
}

function setPrivacyPref(){
  if (document.getElementById('dontShowPrivacyStatement').checked){
    var prefs = getReporterPrefBranch();
    prefs.setBoolPref("hidePrivacyStatement", true);
  }
}

function initForm() {
  var strbundle=document.getElementById("strings");
  var reportWizard = document.getElementById('reportWizard');

  reportWizard.canRewind = false;
  document.getElementById('url').value = gURL;

  // Change next button to "submit report"
  reportWizard.getButton('next').label = strbundle.getString("submitReport") + ">";
  
  // Set the privacy policy link href
  var url = getCharPref("privacyURL", "http://reporter.mozilla.org/privacy/");
  var privacyLink = document.getElementById("privacyPolicy");
  privacyLink.setAttribute("href", url);

  // We don't let the user go forward until they fufill certain requirements - see validateform()
  reportWizard.canAdvance = false;
}

function validateForm() {
  var canAdvance = document.getElementById('problem_type').value != "0";
  document.getElementById('reportWizard').canAdvance = canAdvance;
}

function registerSysID(){
  var param = new Array();;
  param[0] = new SOAPParameter(gLanguage, "language");

  // get sysID
  callReporter("register", param, setValSysID);

  // saving
  if (gSysID != undefined){
    var prefs = getReporterPrefBranch();
    prefs.setCharPref("sysid", gSysID);
    return gSysID;
  }
  return "";
}

function getSysID() {
  var sysId = getCharPref("sysid", "");
  if (sysId == "")
    sysId = registerSysID();
  
  return sysId;
}

function sendReport() {
  // we control the user path from here.
  var reportWizard = document.getElementById('reportWizard');

  reportWizard.canRewind = false;
  reportWizard.canAdvance = false;
  // why would we need a cancel button?
  reportWizard.getButton("cancel").disabled = true;

  var strbundle=document.getElementById("strings");
  var statusDescription = document.getElementById('sendReportProgressDescription');
  var statusIndicator = document.getElementById('sendReportProgressIndicator');

  // Data from form we need
  var descriptionStri = document.getElementById('description').value;
  var problemTypeStri = document.getElementById('problem_type').value;
  var behindLoginStri = document.getElementById('behind_login').checked;
  var emailStri = document.getElementById('email').value;

  var buildConfig = getBuildConfig();
  var userAgent = navigator.userAgent;

  // SOAP params
  var param = new Array();
  param[0] = new SOAPParameter(gRMOvers,            "rmoVers");
  param[1] = new SOAPParameter(gURL,                "url");
  param[2] = new SOAPParameter(problemTypeStri,     "problem_type");
  param[3] = new SOAPParameter(descriptionStri,     "description");
  param[4] = new SOAPParameter(behindLoginStri,     "behind_login");
  param[5] = new SOAPParameter(navigator.platform,  "platform");
  param[6] = new SOAPParameter(navigator.oscpu,     "oscpu");
  param[7] = new SOAPParameter(getGecko(),          "gecko");
  param[8] = new SOAPParameter(getProduct(),        "product");
  param[9] = new SOAPParameter(navigator.userAgent, "useragent");
  param[10] = new SOAPParameter(buildConfig,        "buildconfig");
  param[11] = new SOAPParameter(gLanguage,          "language");
  param[12] = new SOAPParameter(emailStri,          "email");
  param[13] = new SOAPParameter(getSysID(),         "sysid");

  statusIndicator.setAttribute("value", "5%");
  statusDescription.setAttribute("value", strbundle.getString("sendingReport"));
  callReporter("submitReport", param, setValReportID);

  var finishSummary = document.getElementById('finishSummary');
  var finishExtendedFailed = document.getElementById('finishExtendedFailed');
  var finishExtendedSuccess = document.getElementById('finishExtendedSuccess');
  if (!gSOAPerror) {
    // If successful
    finishExtendedFailed.setAttribute("class", "hide");

    statusIndicator.setAttribute("value", "95%");
    statusDescription.setAttribute("value", strbundle.getString("reportSent"));

    reportWizard.canAdvance = true;
    statusIndicator.setAttribute("value", "100%");

    // Send to the finish page
    reportWizard.advance();

    // report ID returned from the web service
    finishSummary.setAttribute("value", strbundle.getString("successfullyCreatedReport") + " " + gReportID);

    finishExtendedDoc = finishExtendedSuccess.contentDocument;
    finishExtendedDoc.getElementById('urlStri').textContent         = gURL;
    finishExtendedDoc.getElementById('problemTypeStri').textContent = document.getElementById('problem_type').label;
    finishExtendedDoc.getElementById('descriptionStri').textContent = descriptionStri;
    finishExtendedDoc.getElementById('platformStri').textContent    = navigator.platform;
    finishExtendedDoc.getElementById('oscpuStri').textContent       = navigator.oscpu;
    finishExtendedDoc.getElementById('productStri').textContent     = getProduct();
    finishExtendedDoc.getElementById('geckoStri').textContent       = getGecko();
    finishExtendedDoc.getElementById('buildConfigStri').textContent = buildConfig;
    finishExtendedDoc.getElementById('userAgentStri').textContent   = navigator.userAgent;
    finishExtendedDoc.getElementById('langStri').textContent        = gLanguage;
    finishExtendedDoc.getElementById('emailStri').textContent       = emailStri;

    reportWizard.canRewind = false;
  } else {
    // If there was an error from the server
    finishExtendedSuccess.setAttribute("class", "hide");

    // Change the label on the page so users know we have an error
    var finishPage = document.getElementById('finish');
    finishPage.setAttribute("label",strbundle.getString("finishError"));

    reportWizard.canAdvance = true;
    reportWizard.advance();

    finishSummary.setAttribute("value",strbundle.getString("failedCreatingReport"));

    finishExtendedDoc = finishExtendedFailed.contentDocument;
    //finishExtendedDoc.getElementById('faultCode').textContent = gFaultCode;
    finishExtendedDoc.getElementById('faultMessage').textContent = gFaultMessage;
  }
  document.getElementById('finishExtendedFrame').collapsed = true;
  reportWizard.canRewind = false;
  reportWizard.getButton("cancel").disabled = true;
}

function showDetail() {
  var hideDetail = document.getElementById('showDetail').checked ? false : true;
  document.getElementById('finishExtendedFrame').collapsed = hideDetail;
}

function getBuildConfig() {
  // bz and Biesi are my heroes for writing/debugging this chunk.
  try {
    netscape.security.PrivilegeManager
            .enablePrivilege("UniversalXPConnect UniversalBrowserRead UniversalBrowserWrite");
    var ioservice =
      Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
    var channel = ioservice.newChannel("chrome://global/content/buildconfig.html", null, null);
    var stream = channel.open();
    var scriptableInputStream =
      Components.classes["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Components.interfaces.nsIScriptableInputStream);
    scriptableInputStream.init(stream);
    var data = "";
    var curBit = scriptableInputStream.read(4096);
    while (curBit.length) {
      data += curBit;
      curBit = scriptableInputStream.read(4096);
    }
    // Strip out the <!DOCTYPE> part, since it's not valid XML
    data = data.replace(/^<!DOCTYPE[^>]*>/, "");
    // Probably not strictly needed, but what the heck
    data = data.replace(/^<html>/, "<html xmlns='http://www.w3.org/1999/xhtml'>");
    var parser = new DOMParser();
    var buildconfig = parser.parseFromString(data, "application/xhtml+xml");
    var text = buildconfig.getElementsByTagName("body")[0].textContent;
    var start= text.indexOf('Configure arguments')+19;
    return text.substring(start);
  } catch(ex) {
    dump(ex);
    return "Unknown";
  }
}

/*  NEW WEB SERVICE MODULE */
/*  Based on Apple's example implementation of SOAP at: developer.apple.com/internet/webservices/mozgoogle_source.html */
function callReporter(method, params, callback) {
  var serviceURL = getCharPref("serviceURL", "http://reporter.mozilla.org/service/");

  var soapCall = new SOAPCall();
  soapCall.transportURI = serviceURL;
  soapCall.encode(0, method, "urn:MozillaReporter", 0, null, params.length, params);

  var response = soapCall.invoke();
  var error = handleSOAPResponse(response);
  if (!error)
    callback(response);
}

function handleSOAPResponse (response) {
  var fault = response.fault;
  if (fault != null) {
    gSOAPerror = true;
    gFaultCode = fault.faultCode;
    gFaultMessage = fault.faultString;
    return true;
  }

  return false;
}

function setValSysID(results) {
  if (results) {
    var params = results.getParameters(false,{});
    for (var i = 0; i < params.length; i++){
      gSysID = params[i].value;
    }
  }
}

function setValReportID(results) {
  if (results) {
    var params = results.getParameters(false,{});
    for (var i = 0; i < params.length; i++){
      gReportID = params[i].value;
    }
  }
}

function getProduct() {
  try {
    // Works on Firefox 1.0+ and Future SeaMonkey's
    var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                            .getService(Components.interfaces.nsIXULAppInfo);
    // Use App info if possible
    return appInfo.name+"/"+appInfo.version;
  }
  catch(ex) {}
  // only works on Gecko 1.8 and higher (if above doesn't return)
  if ('nsIChromeRegistrySea' in Components.interfaces) {
    return 'SeaMonkey/'+
    Components.classes['@mozilla.org/network/io-service;1']
              .getService(Components.interfaces.nsIIOService)
              .getProtocolHandler('http')
              .QueryInterface(Components.interfaces.nsIHttpProtocolHandler).misc.substring(3);
  }
  // Firefox < 1.0+ or a last ditch effort for others that may fail
  else if (navigator.vendor != ''){
    return window.navigator.vendor+'/'+window.navigator.vendorSub;
  }
  else {
    return "Unknown";
  }
}

function getGecko() {
  try {
    var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                            .getService(Components.interfaces.nsIXULAppInfo);
    // Use App info if possible
    return appInfo.platformBuildID;
  }
  catch(ex) {
    return "00000000"; // 8 0's to ignore as we have historically
  }
}
