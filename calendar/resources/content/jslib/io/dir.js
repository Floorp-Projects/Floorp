/*** -*- Mode: Javascript; tab-width: 2;

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

Contributor(s): Pete Collins, Doug Turner, Brendan Eich, Warren Harris,
                Eric Plaster, Martin Kutschker


JS Directory Class API

  dir.js

Function List

    create(aPermissions);      // permissions are optional

    files();                   // returns an array listing all files of a dirs contents 
    dirs();                    // returns an array listing all dirs of a dirs contents 
    list(aDirPath);            // returns an array listing of a dirs contents 

    // help!
    help();                    // currently dumps a list of available functions 

Instructions:


*/

if (typeof(JS_LIB_LOADED)=='boolean') {

/************* INCLUDE FILESYSTEM *****************/
if(typeof(JS_FILESYSTEM_LOADED)!='boolean')
  include(jslib_filesystem);
/************* INCLUDE FILESYSTEM *****************/


/****************** Globals **********************/
const JS_DIR_FILE                    = "dir.js";
const JS_DIR_LOADED                  = true;

const JS_DIR_LOCAL_CID               = "@mozilla.org/file/local;1";
const JS_DIR_LOCATOR_PROGID          = '@mozilla.org/filelocator;1';
const JS_DIR_CID                     = "@mozilla.org/file/directory_service;1";

const JS_DIR_I_LOCAL_FILE            = "nsILocalFile";
const JS_DIR_INIT_W_PATH             = "initWithPath";

const JS_DIR_PREFS_DIR               = 65539;

const JS_DIR_DIRECTORY               = 0x01;     // 1
const JS_DIR_OK                      = true;

const JS_DIR_DEFAULT_PERMS           = 0755;

const JS_DIR_FilePath                = new C.Constructor(JS_DIR_LOCAL_CID, 
                                                   JS_DIR_I_LOCAL_FILE, 
                                                   JS_DIR_INIT_W_PATH);
/****************** Globals **********************/

/****************** Dir Object Class *********************/
// constructor
function Dir(aPath) {

  if(!aPath) {
    jslibError(null,
              "Please enter a local file path to initialize",
              "NS_ERROR_XPC_NOT_ENOUGH_ARGS", JS_DIR_FILE);
    throw C.results.NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  return this.initPath(arguments);
} // end constructor

Dir.prototype = new FileSystem;
Dir.prototype.fileInst = null;

/********************* CREATE ****************************/
Dir.prototype.create = function(aPermissions) 
{
  if(!this.mPath) {
    jslibError(null, "create (no file path defined)", "NS_ERROR_NOT_INITIALIZED");
    return C.results.NS_ERROR_NOT_INITIALIZED;
  }

  if(this.exists()) {
    jslibError(null, "(Dir already exists", "NS_ERROR_FAILURE", JS_DIR_FILE+":create");
    return null;
  }

  if(aPermissions) {
    var checkPerms = this.validatePermissions(aPermissions);

    if(!checkPerms) {
      jslibError(null, "create (invalid permissions)", 
                       "NS_ERROR_INVALID_ARG", JS_DIR_FILE+":create");
      return C.results.NS_ERROR_INVALID_ARG;
    }               

    //var baseTen = permissions.toString(10);
    //if(baseTen.substring(0,1) != 0)
      //aPermissions = 0+baseTen;
  } else {
    checkPerms = JS_DIR_DEFAULT_PERMS;
  }

  var rv = null;

  try {
    rv=this.mFileInst.create(JS_DIR_DIRECTORY, parseInt(aPermissions) );
  } catch (e) { 
    jslibError(e, "(unable to create)", "NS_ERROR_FAILURE", JS_DIR_FILE+":create");
    rv=null;
  }

  return rv;
};

/********************* READDIR **************************/
Dir.prototype.readDir = function ()
{

  if(!this.exists()) {
    jslibError(null, "(Dir already exists", "NS_ERROR_FAILURE", JS_DIR_FILE+":readDir");
    return null;
  }

  var rv=null;

  try {
    if(!this.isDir()) {
      jslibError(null, "(file is not a directory)", "NS_ERROR_FAILURE", JS_DIR_FILE+":readDir");
      return null;
    }

    var files     = this.mFileInst.directoryEntries;
    var listings  = new Array();
    var file;

    if(typeof(JS_FILE_LOADED)!='boolean')
      include(JS_LIB_PATH+'io/file.js');

    while(files.hasMoreElements()) {
      file = files.getNext().QueryInterface(C.interfaces.nsILocalFile);
      if(file.isFile())
        listings.push(new File(file.path));

      if(file.isDirectory())
        listings.push(new Dir(file.path));
    }

    rv=listings;
  } catch(e) { 
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILE_FILE+":readDir");
    rv=null;
  }

  return rv;
};

/********************* REMOVE *******************************/
Dir.prototype.remove = function (aRecursive)
{

  if(typeof(aRecursive)!='boolean')
    aRecursive=false;

  if(!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if(!this.mPath)
  {
    jslibError(null, "remove (no path defined)", 
                     "NS_ERROR_INVALID_ARG", JS_DIR_FILE+":remove");
    return null;
  }

  var rv=null

  try { 
    if(!this.exists()) {
      jslibError(null, "(directory doesn't exist)", "NS_ERROR_FAILURE", JS_DIR_FILE+":remove");
      return null;
    }

    if(!this.isDir()) {
      jslibError(null, "(file is not a directory)", "NS_ERROR_FAILURE", JS_DIR_FILE+":remove");
      return null;
    }

    rv=this.mFileInst.remove(aRecursive);
  } catch (e) { 
    jslibError(e, "(dir not empty, use 'remove(true)' for recursion)", "NS_ERROR_UNEXPECTED", 
                  JS_DIR_FILE+":remove");
    rv=null;
  }

  return rv;
};

/********************* HELP *****************************/
Dir.prototype.super_help = FileSystem.prototype.help;

Dir.prototype.__defineGetter__('help', 
function() {
  var help = this.super_help()              +

    "   create(aPermissions);\n"            +
    "   remove(aRecursive);\n"              +
    "   readDir(aDirPath);\n";

  return help;
});

jslibDebug('*** load: '+JS_DIR_FILE+' OK');

} else {
    dump("JSLIB library not loaded:\n"                                  +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include(jslib_dir);\n\n");
}

