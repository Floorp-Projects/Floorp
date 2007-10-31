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

const gParamLanguage = window.navigator.language;
const gRMOvers = "0.3"; // Do not touch without contacting reporter admin!
const gParamURL = window.arguments[0];
const gParamUserAgent = navigator.userAgent;
const gParamOSCPU = navigator.oscpu;
const gParamPlatform = navigator.platform;

// Globals
var gParamDescription;
var gParamProblemType;
var gParamBehindLogin;
var gParamEmail;
var gParamBuildConfig;
var gParamGecko;

var gPrefBranch;
var gStatusIndicator;

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
  document.getElementById('url').value = gParamURL;

  // Change next button to "submit report"
  reportWizard.getButton('next').label = strbundle.getString("submitReport");
  reportWizard.getButton('next').setAttribute("accesskey",
                                              strbundle.getString("submitReport.accesskey"));


  // Set the privacy policy link href
  var url = getCharPref("privacyURL", "http://reporter.mozilla.org/privacy/");
  var privacyLink = document.getElementById("privacyPolicy");
  privacyLink.setAttribute("href", url);

  // We don't let the user go forward until they fufill certain requirements - see validateform()
  reportWizard.canAdvance = false;

  document.getElementById("problem_type").focus();

}

function validateForm() {
  var canAdvance = document.getElementById('problem_type').value != "0";
  document.getElementById('reportWizard').canAdvance = canAdvance;
}

function registerSysID() {
  var param = {
    'method':             'submitRegister',
    'language':           gParamLanguage
  };

  // go get sysID
  sendReporterServerData(param, onRegisterSysIDLoad);
}

function onRegisterSysIDLoad(req) {
  if (req.status == 200) {
    var paramSysID = req.responseXML.getElementsByTagName('result').item(0);

    // saving
    if (paramSysID) {
      var prefs = getReporterPrefBranch();
      prefs.setCharPref("sysid", paramSysID.textContent);

      // Finally send report
      sendReport();
      return;
    }
    
    // Invalid Response Error
    var strbundle = document.getElementById("strings");
    showError(strbundle.getString("invalidResponse"));

    return;
  }

  // On error
  var errorStr = extractError(req);
  showError(errorStr);
}


function sendReport() {
  // Check for a sysid, if we don't have one, get one it will call sendReport on success.
  var sysId = getCharPref("sysid", "");
  if (sysId == ""){
    registerSysID();
    return;
  }

  // we control the user path from here.
  var reportWizard = document.getElementById('reportWizard');

  reportWizard.canRewind = false;
  reportWizard.canAdvance = false;
  // why would we need a cancel button?
  reportWizard.getButton("cancel").disabled = true;

  var strbundle=document.getElementById("strings");
  var statusDescription = document.getElementById('sendReportProgressDescription');
  gStatusIndicator = document.getElementById('sendReportProgressIndicator');

  // Data from form we need
  gParamDescription = document.getElementById('description').value;
  gParamProblemType = document.getElementById('problem_type').value;
  gParamBehindLogin = document.getElementById('behind_login').checked;
  gParamEmail = document.getElementById('email').value;

  gParamBuildConfig = getBuildConfig();
  gParamGecko = getGecko();

  // SOAP params
  var param = {
    'method':           'submitReport',
    'rmoVers':          gRMOvers,
    'url':              gParamURL,
    'problem_type':     gParamProblemType,
    'description':      gParamDescription,
    'behind_login':     gParamBehindLogin,
    'platform':         gParamPlatform,
    'oscpu':            gParamOSCPU,
    'gecko':            gParamGecko,
    'product':          getProduct(),
    'useragent':        gParamUserAgent,
    'buildconfig':      gParamBuildConfig,
    'language':         gParamLanguage,
    'email':            gParamEmail,
    'sysid':            sysId
  };

  gStatusIndicator.value = "5%";
  statusDescription.value = strbundle.getString("sendingReport");

  sendReporterServerData(param, onSendReportDataLoad);
}

function onSendReportDataProgress(e) {
  gStatusIndicator.value = (e.position / e.totalSize)*100;
}

function sendReporterServerData(params, callback) {
  var serviceURL = getCharPref("serviceURL", "http://reporter.mozilla.org/service/0.3/");

  params = serializeParams(params);

  var request = new XMLHttpRequest();
  request.onprogress = onSendReportDataProgress;
  request.open("POST", serviceURL, true);

  request.onreadystatechange = function () {
    if (request.readyState == 4)
      callback(request);
  };

  request.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  request.setRequestHeader("Content-length", params.length);
  request.setRequestHeader("Connection", "close");
  request.send(params);
}

function serializeParams(params) {
  var str = '';
  for (var key in params) {
    str += key + '=' + encodeURIComponent(params[key]) + '&';
  }
  return str.substr(0, str.length-1);
}

function onSendReportDataLoad(req) {
  if (req.status != 200) {
    var errorStr = extractError(req);
    showError(errorStr);
    return;
  }

  var reportWizard = document.getElementById('reportWizard');

  var finishSummary = document.getElementById('finishSummary');
  var finishExtendedFailed = document.getElementById('finishExtendedFailed');
  var finishExtendedSuccess = document.getElementById('finishExtendedSuccess');
  var statusDescription = document.getElementById('sendReportProgressDescription');

  var strbundle = document.getElementById("strings");

  // If successful
  finishExtendedFailed.hidden = true;

  statusDescription.value = strbundle.getString("reportSent");

  reportWizard.canAdvance = true;
  gStatusIndicator.value = "100%";

  // Send to the finish page
  reportWizard.advance();

  // report ID returned from the web service
  var reportId = req.responseXML.getElementsByTagName('reportId').item(0).firstChild.data;
  finishSummary.value = strbundle.getString("successfullyCreatedReport") + " " + reportId;

  finishExtendedDoc = finishExtendedSuccess.contentDocument;
  finishExtendedDoc.getElementById('urlStri').textContent         = gParamURL;
  finishExtendedDoc.getElementById('problemTypeStri').textContent = document.getElementById('problem_type').label;
  finishExtendedDoc.getElementById('descriptionStri').textContent = gParamDescription;
  finishExtendedDoc.getElementById('platformStri').textContent    = gParamPlatform;
  finishExtendedDoc.getElementById('oscpuStri').textContent       = gParamOSCPU;
  finishExtendedDoc.getElementById('productStri').textContent     = getProduct();
  finishExtendedDoc.getElementById('geckoStri').textContent       = gParamGecko;
  finishExtendedDoc.getElementById('buildConfigStri').textContent = gParamBuildConfig;
  finishExtendedDoc.getElementById('userAgentStri').textContent   = gParamUserAgent;
  finishExtendedDoc.getElementById('langStri').textContent        = gParamLanguage;
  finishExtendedDoc.getElementById('emailStri').textContent       = gParamEmail;

  reportWizard.canRewind = false;

  document.getElementById('finishExtendedFrame').collapsed = true;
  reportWizard.getButton("cancel").disabled = true;
  return;
}

function extractError(req){
  var error = req.responseXML.getElementsByTagName('errorString').item(0)
  if (error) {
    return error.textContent;
  }

  // Default error
  var strbundle = document.getElementById("strings");
  return strbundle.getString("defaultError");
}

function showError(errorStr){
  var strbundle = document.getElementById("strings");
  var finishSummary = document.getElementById('finishSummary');
  var finishExtendedSuccess = document.getElementById('finishExtendedSuccess');
  var finishExtendedFailed = document.getElementById('finishExtendedFailed');

  // If there was an error from the server
  finishExtendedSuccess.hidden = true;

  // Change the label on the page so users know we have an error
  var finishPage = document.getElementById('finish');
  finishPage.setAttribute("label",strbundle.getString("finishError"));

  var reportWizard = document.getElementById('reportWizard');
  reportWizard.canAdvance = true;
  reportWizard.advance();

  finishSummary.value = strbundle.getString("failedCreatingReport");

  finishExtendedDoc = finishExtendedFailed.contentDocument;
  finishExtendedDoc.getElementById('faultMessage').textContent = errorStr;

  document.getElementById('finishExtendedFrame').collapsed = true;
  reportWizard.getButton("cancel").disabled = true;
}

function showDetail() {
  var hideDetail = document.getElementById('showDetail').checked ? false : true;
  document.getElementById('finishExtendedFrame').collapsed = hideDetail;
}

function getBuildConfig() {
  // bz and Biesi are my heroes for writing/debugging this chunk.
  try {
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
