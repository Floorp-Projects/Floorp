/*** -*- Mode: Javascript; tab-width: 2; -*-

The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Collabnet code.
The Initial Developer of the Original Code is Collabnet.

Portions created by Collabnet are Copyright (C) 2000 Collabnet.
All Rights Reserved.

Contributor(s): Pete Collins,
                Doug Turner,
                Brendan Eich,
                Warren Harris,
                Eric Plaster,
                Martin Kutschker
                Philip Lindsay


JS FileUtils IO API (The purpose of this file is to make it a little easier to do file IO from js) 

    fileUtils.js

Function List

    chromeToPath(aPath)              // Converts a chrome://bob/content uri to a path.
                                     //  NOTE: although this gives you the
                                     //         path to a file in the chrome directory, you will
                                     // most likely not have permisions
                                     // to create or write to files there.
    urlToPath(aPath)                 // Converts a file:// url to a path
    exists(aPath);                   // check to see if a file exists
    append(aDirPath, aFileName);     // append is for abstracting platform specific file paths
    remove(aPath);                   // remove a file
    copy(aSource, aDest);            // copy a file from source to destination
    leaf(aPath);                     // leaf is the endmost file string
                                     //  eg: foo.html in /myDir/foo.html
    permissions(aPath);              // returns the files permissions
    dateModified(aPath);             // returns the last modified date in locale string
    size(aPath);                     // returns the file size
    ext(aPath);                      // returns a file extension if there is one
    parent(aPath)                    // returns the dir part of a path
    dirPath(aPath)                   // *Depriciated* use parent 
    spawn(aPath, aArgs)              // spawns another program 
    nsIFile(aPath)                   // returns an nsIFile obj 
    help;                            // currently returns a list of available functions 

  Deprecated

    chrome_to_path(aPath);           // synonym for chromeToPath
    URL_to_path(aPath)               // synonym for use urlToPath
    rm(aPath);                       // synonym for remove
    extension(aPath);                // synonym for ext

Instructions:

  First include this js file 

   var file = new FileUtils();

  Examples:

   var path='/usr/X11R6/bin/Eterm';
   file.spawn(path, ['-e/usr/bin/vi']); 
   *note* all args passed to spawn must be in the form of an array

   // to list help
   dump(file.help);

  Warning: these API's are not for religious types

*/

// Make sure jslib is loaded
if (typeof(JS_LIB_LOADED)=='boolean')
{

/****************** Globals **********************/

const JS_FILEUTILS_FILE                    = "fileUtils.js";
const JS_FILEUTILS_LOADED                  = true;

const JS_FILEUTILS_LOCAL_CID               = "@mozilla.org/file/local;1";
const JS_FILEUTILS_FILESPEC_PROGID         = '@mozilla.org/filespec;1';
const JS_FILEUTILS_NETWORK_STD_CID         = '@mozilla.org/network/standard-url;1';
const JS_FILEUTILS_SIMPLEURI_PROGID        = "@mozilla.org/network/simple-uri;1";
const JS_FILEUTILS_CHROME_REG_PROGID       = '@mozilla.org/chrome/chrome-registry;1';
const JS_FILEUTILS_DR_PROGID               = "@mozilla.org/file/directory_service;1";
const JS_FILEUTILS_PROCESS_CID             = "@mozilla.org/process/util;1";

const JS_FILEUTILS_I_LOCAL_FILE            = "nsILocalFile";
const JS_FILEUTILS_INIT_W_PATH             = "initWithPath";
const JS_FILEUTILS_I_PROPS                 = "nsIProperties";

const JS_FILEUTILS_CHROME_DIR              = "AChrom";

const JS_FILEUTILS_OK                      = true;
const JS_FILEUTILS_FilePath                = new 
C.Constructor(JS_FILEUTILS_LOCAL_CID, JS_FILEUTILS_I_LOCAL_FILE, JS_FILEUTILS_INIT_W_PATH);

const JS_FILEUTILS_I_URI                   = C.interfaces.nsIURI;
const JS_FILEUTILS_I_FILEURL               = C.interfaces.nsIFileURL;
const JS_FILEUTILS_I_PROCESS               = C.interfaces.nsIProcess;

/****************** FileUtils Object Class *********************/
function FileUtils() {
  include (jslib_dirutils);
  this.mDirUtils = new DirUtils();
} // constructor

FileUtils.prototype  = {

  mFileInst        : null,
  mDirUtils        : null,

/********************* CHROME_TO_PATH ***************************/
// this is here for backward compatability but is deprecated --pete
chrome_to_path : function (aPath) { return this.chromeToPath(aPath); },

chromeToPath : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":chromeToPath");
    return null;
  }

  var uri = C.classes[JS_FILEUTILS_SIMPLEURI_PROGID].createInstance(JS_FILEUTILS_I_URI);
  var rv;

  if (/^chrome:/.test(aPath)) {
    try {
      var cr        = C.classes[JS_FILEUTILS_CHROME_REG_PROGID].getService();
      if (cr) {
        cr          = cr.QueryInterface(C.interfaces.nsIChromeRegistry);
        uri.spec    = aPath;
        uri.spec    = cr.convertChromeURL(uri);
        rv         = uri.path;
      }
    } catch(e) {}

    if (/^\/|\\|:chrome/.test(rv)) {
      try {
        // prepend the system path to this process dir
        rv        = "file://"+this.mDirUtils.getCurProcDir()+rv;
      } catch (e) {
        jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", 
                        JS_FILEUTILS_FILE+":chromeToPath");
        rv        = "";
      }
    }
  } 

  else if (/^file:/.test(aPath)) {
      rv = this.urlToPath(aPath); 
  } else
      rv = "";
  
  return rv;
},

/********************* URL_TO_PATH ***************************/
URL_to_path : function (aPath){ return this.urlToPath(aPath); },

urlToPath : function (aPath)
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":urlToPath");
    return null;
  }

  var rv;
  if (aPath.search(/^file:/) == 0) {
    try {
     var uri = C.classes[JS_FILEUTILS_NETWORK_STD_CID].createInstance(JS_FILEUTILS_I_FILEURL);
     uri.spec = aPath;
     rv = uri.file.path;
    } catch (e) { 
      jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":urlToPath");
      rv=null;
    }
  }

  return rv;
},

/********************* EXISTS ***************************/
exists : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":exists");
    return null;
  }

  var rv;
  try { 
    var file                = new JS_FILEUTILS_FilePath(aPath);
    rv=file.exists();
  } catch(e) { 
    jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":exists");
    rv=null;
  }

  return rv;
},

/********************* RM *******************************/
rm : function (aPath) { return this.remove(aPath); },

remove : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":remove");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":remove");
    return null;
  }

  var rv;

  try { 
    var fileInst = new JS_FILEUTILS_FilePath(aPath);
    if (fileInst.isDirectory()) {
      jslibError(null, "path is a dir. use rmdir()", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":remove");
      return null;
    }

    fileInst.remove(false);
    rv = C.results.NS_OK;
  } catch (e) { 
    jslibError(e, "(unexpected)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":urlToPath");
    rv=null;
  }

  return rv;
},

/********************* COPY *****************************/
copy  : function (aSource, aDest) 
{
  if (!aSource || !aDest) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":copy");
    return null;
  }

  if (!this.exists(aSource)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":copy");
    return null;
  }

  var rv;

  try { 
    var fileInst      = new JS_FILEUTILS_FilePath(aSource);
    var dir           = new JS_FILEUTILS_FilePath(aDest);
    var copyName      = fileInst.leafName;

    if (fileInst.isDirectory()) {
      jslibError(null, "(cannot copy directory)", "NS_ERROR_FAILURE", JS_FILEUTILS_FILE+":copy");
      return null;
    }

    if (!this.exists(aDest) || !dir.isDirectory()) {
      copyName          = dir.leafName;
      dir               = new JS_FILEUTILS_FilePath(dir.path.replace(copyName,''));

      if (!this.exists(dir.path)) {
        jslibError(null, "(dest "+dir.path+" doesn't exist)", "NS_ERROR_FAILURE", JS_FILEUTILS_FILE+":copy");
        return null;
      }

      if (!dir.isDirectory()) {
        jslibError(null, "(dest "+dir.path+" is not a valid path)", "NS_ERROR_FAILURE", JS_FILEUTILS_FILE+":copy");
        return null;
      }
    }

    if (this.exists(this.append(dir.path, copyName))) {
      jslibError(null, "(dest "+this.append(dir.path, copyName)+" already exists)", "NS_ERROR_FAILURE", JS_FILEUTILS_FILE+":copy");
      return null;
    }

    rv=fileInst.copyTo(dir, copyName);
    rv = C.results.NS_OK;
  } catch (e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":copy");
    rv=null;
  }

  return rv;
},

/********************* LEAF *****************************/
leaf  : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":leaf");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":leaf");
    return null;
  }

  var rv;

  try {
    var fileInst = new JS_FILEUTILS_FilePath(aPath);
    rv=fileInst.leafName;
  }

  catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":leaf");
    rv=null;
  }

  return rv;
},

/********************* APPEND ***************************/
append : function (aDirPath, aFileName) 
{
  if (!aDirPath || !aFileName) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":append");
    return null;
  }

  if (!this.exists(aDirPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":append");
    return null;
  }

  var rv;

  try { 
    var fileInst  = new JS_FILEUTILS_FilePath(aDirPath);
    if (fileInst.exists() && !fileInst.isDirectory()) {
      jslibError(null, aDirPath+" is not a dir", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":append");
      return null;
    }

    fileInst.append(aFileName);
    rv=fileInst.path;
    delete fileInst;
  } catch(e) { 
    jslibError(e?e:null, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":append");
    rv=null;
  }

  return rv;
},

/********************* VALIDATE PERMISSIONS *************/
validatePermissions : function(aNum) 
{
  if ( parseInt(aNum.toString(10).length) < 3 ) 
    return false;

  return JS_FILEUTILS_OK;
},

/********************* PERMISSIONS **********************/
permissions : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":permissions");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":permissions");
    return null;
  }

  var rv;

  try { 
    rv=(new JS_FILEUTILS_FilePath(aPath)).permissions.toString(8);
  } catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":permissions");
    rv=null;
  }

  return rv;
},

/********************* MODIFIED *************************/
dateModified  : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":dateModified");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":dateModified");
    return null;
  }

  var rv;

  try { 
    var date = new Date((new JS_FILEUTILS_FilePath(aPath)).lastModificationDate).toLocaleString();
    rv=date;
  } catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":dateModified");
    rv=null;
  }

  return rv;
},

/********************* SIZE *****************************/
size  : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":size");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":size");
    return null;
  }

  var rv;

  try { 
    rv = (new JS_FILEUTILS_FilePath(aPath)).fileSize;
  } catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":size");
    rv=0;
  }

  return rv;
},

/********************* EXTENSION ************************/
extension  : function (aPath){ return this.ext(aPath); },

ext  : function (aPath)
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":ext");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":ext");
    return null;
  }

  var rv;

  try { 
    var leafName  = (new JS_FILEUTILS_FilePath(aPath)).leafName;
    var dotIndex  = leafName.lastIndexOf('.'); 
    rv=(dotIndex >= 0) ? leafName.substring(dotIndex+1) : ""; 
  } catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":ext");
    rv=null;
  }

  return rv;
},

/********************* DIRPATH **************************/
dirPath   : function (aPath){ return this.parent(aPath); }, 

parent   : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":parent");
    return null;
  }

  var rv;

  try { 
    var fileInst            = new JS_FILEUTILS_FilePath(aPath);

    if (!fileInst.exists()) {
      jslibError(null, "(file doesn't exist)", "NS_ERROR_FAILURE", JS_FILEUTILS_FILE+":parent");
      return null;
    }

    if (fileInst.isFile())
      rv=fileInst.parent.path;

    else if (fileInst.isDirectory())
      rv=fileInst.path;

    else
      rv=null;
  }

  catch (e) { 
    jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":parent");
    rv=null;
  }

  return rv;
},

/********************* SPAWN ****************************/
run : function (aPath, aArgs) { this.spawn(aPath, aArgs); },
spawn : function (aPath, aArgs) 
/*
 * Trys to execute the requested file as a separate *non-blocking* process.
 * 
 * Passes the supplied *array* of arguments on the command line if
 * the OS supports it.
 *
 */
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", 
		           JS_FILEUTILS_FILE+":spawn");
    return null;
  }

  if (!this.exists(aPath)) {
    jslibError(null, "(file doesn't exist)", "NS_ERROR_UNEXPECTED", 
		           JS_FILEUTILS_FILE+":spawn");
    return null;
  }

  var len=0;

  if (aArgs)
    len = aArgs.length;
  else
    aArgs=null;

  var rv;

  try { 
    var fileInst            = new JS_FILEUTILS_FilePath(aPath);

    if (!fileInst.isExecutable()) {
      jslibError(null, "(File is not executable)", "NS_ERROR_INVALID_ARG", 
			           JS_FILEUTILS_FILE+":spawn");
      return null;
    }

    if (fileInst.isDirectory()) {
      jslibError(null, "(File is not a program)", "NS_ERROR_UNEXPECTED", 
			           JS_FILEUTILS_FILE+":spawn");
      return null;
    } else {
      // Create and execute the process...
      /*
       * NOTE: The first argument of the process instance's 'run' method
       *       below specifies the blocking state (false = non-blocking).
       *       The last argument, in theory, contains the process ID (PID)
       *       on return if a variable is supplied--not sure how to implement
       *       this with JavaScript though.
       */
      try {
        var theProcess = C.classes[JS_FILEUTILS_PROCESS_CID].
				                 createInstance(JS_FILEUTILS_I_PROCESS);
        
        theProcess.init(fileInst);

        rv = theProcess.run(false, aArgs, len);
				jslib_debug("rv="+rv);
      } catch (e) {
        jslibError(e, "(problem spawing process)", "NS_ERROR_UNEXPECTED", 
				           JS_FILEUTILS_FILE+":spawn");
        rv=null;
      }
    }
  } catch (e) { 
    jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", 
		           JS_FILEUTILS_FILE+":spawn");
    rv=null;
  }

  return rv;
},

/********************* nsIFILE **************************/
nsIFile : function (aPath) 
{
  if (!aPath) {
    jslibError(null, "(no path defined)", "NS_ERROR_INVALID_ARG", JS_FILEUTILS_FILE+":nsIFile");
    return null;
  }

  var rv;

  try {
    rv = new JS_FILEUTILS_FilePath(aPath);
  } catch (e) { 
    jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", JS_FILEUTILS_FILE+":nsIFile");
    rv = null;
  }

  return rv;
},

/********************* HELP *****************************/
get help() 
{
  var help =

    "\n\nFunction List:\n"                  +
    "\n"                                    +
    "   exists(aPath);\n"                   +
    "   chromeToPath(aPath);\n"             +
    "   urlToPath(aPath);\n"                +
    "   append(aDirPath, aFileName);\n"     +
    "   remove(aPath);\n"                   +
    "   copy(aSource, aDest);\n"            +
    "   leaf(aPath);\n"                     +
    "   permissions(aPath);\n"              +
    "   dateModified(aPath);\n"             +
    "   size(aPath);\n"                     +
    "   ext(aPath);\n"                      +
    "   parent(aPath);\n"                   + 
    "   run(aPath, aArgs);\n"               + 
    "   nsIFile(aPath);\n"                  + 
    "   help;\n";

  return help;
}

};

jslibDebug('*** load: '+JS_FILEUTILS_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

// If jslib base library is not loaded, dump this error.
else
{
    dump("JS_FILE library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/io/fileUtils.js');\n\n");
}

