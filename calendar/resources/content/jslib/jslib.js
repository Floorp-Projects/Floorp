/*** -*- Mode: Javascript; tab-width: 2;

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is jslib team code.
The Initial Developer of the Original Code is jslib team.

Portions created by jslib team are
Copyright (C) 2000 jslib team.  All
Rights Reserved.

Original Author: Pete Collins <pete@mozdev.org>
Contributor(s): Martin Kutschker <Martin.T.Kutschker@blackbox.net>

***/

/** 
 * insure jslib base is not already loaded
 */
if (typeof(JS_LIB_LOADED)!='boolean') {
try {

/*************************** GLOBALS ***************************/
const JS_LIB_LOADED     = true;

const JS_LIBRARY        = "jslib";
const JS_LIB_FILE       = "jslib.js"
const JS_LIB_PATH       = "chrome://calendar/content/jslib/";
const JS_LIB_VERSION    = "0.1.123";
const JS_LIB_AUTHORS    = "\tPete Collins       <petejc@mozdevgroup.com>\n"     +
                          "\tEric Plaster       <plaster@urbanrage.com>\n"    +
                          "\tMartin.T.Kutschker <Martin.T.Kutschker@blackbox.net>\n";
const JS_LIB_BUILD      = "mozilla 1.3+";
const JS_LIB_ABOUT      = "\tThis is an effort to provide a fully "   +      
                          "functional js library\n"                   +
                          "\tfor mozilla package authors to use "     + 
                          "in their applications\n";
const JS_LIB_HOME       = "http://jslib.mozdev.org/";

// Hopefully there won't be any global namespace collisions here
const ON                = true;
const OFF               = false;
const C                 = Components;
const jslib_results     = C.results;

if (typeof(JS_LIB_DEBUG)!='boolean')
  var JS_LIB_DEBUG      = ON;
var JS_LIB_DEBUG_ALERT  = OFF;
var JS_LIB_ERROR        = ON;
var JS_LIB_ERROR_ALERT  = OFF;

const JS_LIB_HELP       = "\n\nWelcome to jslib version "+JS_LIB_VERSION+"\n\n" 
                          + "Global Constants:\n\n"                               
                          + "JS_LIBRARY     \n\t"+JS_LIBRARY     +"\n"
                          + "JS_LIB_FILE    \n\t"+JS_LIB_FILE    +"\n"                 
                          + "JS_LIB_PATH    \n\t"+JS_LIB_PATH    +"\n"
                          + "JS_LIB_VERSION \n\t"+JS_LIB_VERSION +"\n"
                          + "JS_LIB_AUTHORS \n"  +JS_LIB_AUTHORS
                          + "JS_LIB_BUILD   \n\t"+JS_LIB_BUILD   +"\n" 
                          + "JS_LIB_ABOUT   \n"  +JS_LIB_ABOUT
                          + "JS_LIB_HOME    \n\t"+JS_LIB_HOME    +"\n\n"
                          + "Global Variables:\n\n"            
                          + "  JS_LIB_DEBUG\n  JS_LIB_ERROR\n\n";

// help identifier
const jslib_help = "need to write some global help docs here\n";

// Library Identifiers

// io library modules
const jslib_io         = JS_LIB_PATH+'io/io.js';
const jslib_filesystem = JS_LIB_PATH+'io/filesystem.js'
const jslib_file       = JS_LIB_PATH+'io/file.js';
const jslib_fileutils  = JS_LIB_PATH+'io/fileUtils.js';
const jslib_dir        = JS_LIB_PATH+'io/dir.js';
const jslib_dirutils   = JS_LIB_PATH+'io/dirUtils.js';

// data structures
const jslib_dictionary       = JS_LIB_PATH+'ds/dictionary.js';
const jslib_chaindictionary  = JS_LIB_PATH+'ds/chainDictionary.js';

// RDF library modules
const jslib_rdf           = JS_LIB_PATH+'rdf/rdf.js';
const jslib_rdffile       = JS_LIB_PATH+'rdf/rdfFile.js';
const jslib_rdfcontainer  = JS_LIB_PATH+'rdf/rdfContainer.js';
const jslib_rdfresource   = JS_LIB_PATH+'rdf/rdfResource.js';

// network library modules
const jslib_remotefile  = JS_LIB_PATH+'network/remoteFile.js';
const jslib_socket      = JS_LIB_PATH+'network/socket.js';

// network - http
const jslib_http                = JS_LIB_PATH+'network/http.js';
const jslib_getrequest          = JS_LIB_PATH+'network/getRequest.js';
const jslib_postrequest         = JS_LIB_PATH+'network/postRequest.js';
const jslib_multipartrequest    = JS_LIB_PATH+'network/multipartRequest.js';
const jslib_filepart            = JS_LIB_PATH+'network/parts/filePart.js';
const jslib_textpart            = JS_LIB_PATH+'network/parts/textPart.js';
const jslib_urlparameterspart   = JS_LIB_PATH+'network/parts/urlParametersPart.js';
const jslib_bodyparameterspart  = JS_LIB_PATH+'network/parts/bodyParametersPart.js';


// xul dom library modules
const jslib_dialog      = JS_LIB_PATH+'xul/commonDialog.js';
const jslib_filepicker  = JS_LIB_PATH+'xul/commonFilePicker.js';
const jslib_window      = JS_LIB_PATH+'xul/commonWindow.js';
const jslib_routines    = JS_LIB_PATH+'xul/appRoutines.js';

// sound library modules
const jslib_sound = JS_LIB_PATH+'sound/sound.js';

// utils library modules
const jslib_date     = JS_LIB_PATH+'utils/date.js';
const jslib_prefs    = JS_LIB_PATH+'utils/prefs.js';
const jslib_validate = JS_LIB_PATH+'utils/validate.js';

// zip
const jslib_zip  = JS_LIB_PATH+'zip/zip.js';

// install/uninstall
const jslib_install    = JS_LIB_PATH+'install/install.js';
const jslib_uninstall  = JS_LIB_PATH+'install/uninstall.js';

/*************************** GLOBALS ***************************/

/****************************************************************
* void include(aScriptPath)                                     *
* aScriptPath is an argument of string lib chrome path          *
* returns NS_OK on success, 1 if file is already loaded and     *
* - errorno or throws exception on failure                      *
*   Ex:                                                         * 
*       var path='chrome://jslib/content/io/file.js';           *
*       include(path);                                          *
*                                                               *
*   outputs: void(null)                                         *
****************************************************************/

function include(aScriptPath) {

  jslibPrint(aScriptPath);
  if (!aScriptPath) {
    jslibError(null, "Missing file path argument\n", 
                      "NS_ERROR_XPC_NOT_ENOUGH_ARGS", 
                      JS_LIB_FILE+": include");
    throw - C.results.NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  if (aScriptPath==JS_LIB_PATH+JS_LIB_FILE) {
    jslibError(null, aScriptPath+" is already loaded!", 
        "NS_ERROR_INVALID_ARG", JS_LIB_FILE+": include");
    throw - C.results.NS_ERROR_INVALID_ARG;
  }

  var start   = aScriptPath.lastIndexOf('/') + 1;
  var end     = aScriptPath.lastIndexOf('.');
  var slice   = aScriptPath.length - end;
  var loadID  = aScriptPath.substring(start, (aScriptPath.length - slice));
  if (typeof(this['JS_'+loadID.toUpperCase()+'_LOADED']) == 'boolean') {
    jslibPrint (loadID+" library already loaded");
    return 1;
  }

  var rv;
  try {
    const PROG_ID   = "@mozilla.org/moz/jssubscript-loader;1";
    const INTERFACE = "mozIJSSubScriptLoader";
    jslibGetService(PROG_ID, INTERFACE).loadSubScript(aScriptPath);
    rv = C.results.NS_OK;
  } catch (e) {
    jslibDebug(e);
    const msg = aScriptPath+" is not a valid path or is already loaded";
    jslibError(e, msg, "NS_ERROR_INVALID_ARG", JS_LIB_FILE+": include");
    rv = - C.results.NS_ERROR_INVALID_ARG;
  }
  return rv;
}

/****************************************************************
* void jslibDebug(aOutString)                                  *
* aOutString is an argument of string debug message             *
* returns void                                                  *
*   Ex:                                                         * 
*       var msg='Testing function';                             *
*       jslibDebug(msg);                                        *
*                                                               *
*   outputs: Testing function                                   *
****************************************************************/

// this is here for backward compatability but is deprecated --masi
function jslib_debug(aOutString) { return jslibDebug(aOutString); }

function jslibDebug(aOutString) {

  if (!JS_LIB_DEBUG)
    return; 

  if (JS_LIB_DEBUG_ALERT)
    alert(aOutString);

  dump(aOutString+'\n');
  return;
}

// print to stdout
function jslibPrint(aOutString) {
  return (dump(aOutString+'\n'));
}

// Welcome message
jslibDebug(JS_LIB_HELP);
jslibDebug("\n\n*********************\nJS_LIB DEBUG IS ON\n*********************\n\n");


/****************************************************************
* void jslibError(e, aType, aResults, aCaller)                  *
* e        - argument of results exception                      *
* aType    - argument of string error type message              *
* aResults - argument of string Components.results name         *
* aCaller  - argument of string caller filename and func name   *
* returns void                                                  *
*   Ex:                                                         * 
*       jslibError(null, "Missing file path argument\n",        *
*                 "NS_ERROR_XPC_NOT_ENOUGH_ARGS",               *
*                 JS_LIB_FILE+": include");                     *
*                                                               *
*   outputs:                                                    *
*       -----======[ ERROR ]=====-----                          *
*       Error in jslib.js: include:  Missing file path argument *
*                                                               *
*       NS_ERROR_NUMBER:   NS_ERROR_XPC_NOT_ENOUGH_ARGS         *
*       ------------------------------                          *
*                                                               *
****************************************************************/

function jslibError(e, aType, aResults, aCaller) {

  if (!JS_LIB_ERROR)
    return void(null);

  if (arguments.length==0)
    return (dump("JS_LIB_ERROR=ON\n"));

  var errMsg="ERROR: "+(aCaller?"in "+aCaller:"")+"  "+aType+"\n";
  if (e && typeof(e)=='object') {
    var m, n, r, l, ln, fn = "";
    try {
      r  = e.result;
      m  = e.message;
      fn = e.filename;
      l  = e.location; 
      ln = l.lineNumber; 
    } catch (e) {}
    errMsg+="Name:              "+e.name+"\n"       +
            "Result:            "+r+"\n"            +
            "Message:           "+m+"\n"            +
            "FileName:          "+fn+"\n"           +
            "LineNumber:        "+ln+"\n";
  }
  if (aResults)
    errMsg+="NS_ERROR_NUMBER:   "+aResults+"\n";

  if (JS_LIB_ERROR_ALERT)
    alert(errMsg);

  errMsg = "\n-----======[ ERROR ]=====-----\n" + errMsg;
  errMsg += "------------------------------\n\n";

  return (dump(errMsg));
}

function jslibGetService (aURL, aInterface) {
  var rv;
  try {
    rv =  C.classes[aURL].getService(C.interfaces[aInterface]);
  } catch (e) {
    jslibDebug("Error getting service: " + aURL + ", " + aInterface + "\n" + e);
    rv = -1;
  }
  return rv;
}
 
function jslibCreateInstance (aURL, aInterface) {
  var rv;
  try {
    rv = C.classes[aURL].createInstance(C.interfaces[aInterface]);
  } catch (e) {
    jslibDebug("Error creating instance: " + aURL + ", " + aInterface + "\n" + e);
    rv = -1;
  }
  return rv;
}
 
function jslibGetInterface (aInterface) {
  var rv;
  try {
    rv = C.interfaces[aInterface];
  } catch (e) {
    jslibDebug("Error getting interface: [" + aInterface + "]\n" + e);
    rv = -1;
  }
  return rv;
}
 
/************
   QI: function(aEl, aIName)
   {
     try {
       return aEl.QueryInterface(Components.interfaces[aIName]);
     } catch (ex) {
       throw("Unable to QI " + aEl + " to " + aIName);
     }
   }
************/

function jslibUninstall (aPackage, aCallback)
{
  if (!aPackage || typeof(aPackage) != "string")
    throw jslib_results.NS_ERROR_INVALID_ARG;

  include (jslib_window);
  var win = new CommonWindow(null, 400, 400);
  win.position = JS_MIDDLE_CENTER;
  win.openUninstallWindow(aPackage, aCallback);
}

/*********** Launch JSLIB Splash ***************/
function jslibLaunchSplash ()
{
  include (jslib_window);
  const url = "chrome://jslib/content/splash.xul";
  var win = new CommonWindow(url, 400, 220);
  win.position = JS_MIDDLE_CENTER;
  win.openSplash();
}

function jslib_turnDumpOn () {
  include (jslib_prefs);
  // turn on dump
  var pref = new Prefs();
  const prefStr = "browser.dom.window.dump.enabled"
                                                                                                    
  // turn dump on if not enabled
  if (!pref.getBool(prefStr)) {
    pref.setBool(prefStr, true);
    pref.save();
  } 

  return;
}

function jslib_turnDumpOff () {
  include (jslib_prefs);
  // turn off dump
  var pref = new Prefs();
  const prefStr = "browser.dom.window.dump.enabled"
                                                                                                    
  // turn dump off if enabled
  if (pref.getBool(prefStr)) {
    pref.setBool(prefStr, false);
    pref.save();
  } 
  
  return;
}

} catch (e) {}

} // end jslib load test
