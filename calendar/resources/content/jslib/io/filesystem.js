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

Portions created by Collabnet are
Copyright (C) 2000 Collabnet.  All
Rights Reserved.

Contributor(s): Pete Collins, 
                Doug Turner, 
                Brendan Eich, 
                Warren Harris, 
                Eric Plaster,
                Martin Kutschker
***/

if (typeof(JS_LIB_LOADED)=='boolean') {

/***************************
* Globals                  *
***************************/

const JS_FILESYSTEM_LOADED = true;
const JS_FILESYSTEM_FILE   = "filesystem.js";
const JS_FS_LOCAL_CID      = "@mozilla.org/file/local;1";
const JS_FS_DIR_CID        = "@mozilla.org/file/directory_service;1";
const JS_FS_NETWORK_CID    = '@mozilla.org/network/standard-url;1';
const JS_FS_URL_COMP       = "nsIURL";
const JS_FS_I_LOCAL_FILE   = "nsILocalFile";
const JS_FS_DIR_I_PROPS    = "nsIProperties";
const JS_FS_INIT_W_PATH    = "initWithPath";
const JS_FS_CHROME_DIR     = "AChrom";
const JS_FS_USR_DEFAULT    = "DefProfRt";
const JS_FS_PREF_DIR       = "PrefD";
const JS_FS_OK             = true;
const JS_FS_File_Path      = new Components.Constructor
  ( JS_FS_LOCAL_CID, JS_FS_I_LOCAL_FILE, JS_FS_INIT_W_PATH);
const JS_FS_Dir            = new Components.Constructor
  (JS_FS_DIR_CID, JS_FS_DIR_I_PROPS);
const JS_FS_URL            = new Components.Constructor
  (JS_FS_NETWORK_CID, JS_FS_URL_COMP);

/***************************
* Globals                  *
***************************/

/***************************
* FileSystem Object Class  *
***************************/
function FileSystem(aPath) {

  return (aPath?this.initPath(arguments):void(null));

} // constructor

/***************************
* FileSystem Prototype     *
***************************/
FileSystem.prototype  = {

  mPath           : null,
  mFileInst       : null,


/***************************
* INIT PATH                *
***************************/
initPath : function(args)
{

   // check if the argument is a file:// url
   if(typeof(args)=='object') {
      for (var i=0; i<args.length; i++) {
         if(args[i].search(/^file:/) == 0) {
            try {
               var fileURL= new JS_FS_URL();
               fileURL.spec=args[i];
               args[i] = fileURL.path;
            } catch (e) { 
               jslibError(e, "(problem getting file instance)", 
                     "NS_ERROR_UNEXPECTED", JS_FILESYSTEM_FILE+":initPath");
               rv=null;
            }
         }
      }
   } else {
      if(args.search(/^file:/) == 0) {
         try {
            var fileURL= new JS_FS_URL();
            fileURL.spec=args;
            args = fileURL.path;
         } catch (e) { 
            jslibError(e, "(problem getting file instance)", 
                  "NS_ERROR_UNEXPECTED", JS_FILESYSTEM_FILE+":initPath");
            rv=null;
         }
      }
   }


  /** 
   * If you are wondering what all this extra cruft is, well
   * this is here so you can reinitialize 'this' with a new path
   */
  var rv = null;
  try {
    if (typeof(args)=='object') {
      this.mFileInst = new JS_FS_File_Path(args[0]?args[0]:this.mPath);
      if (args.length>1)
        for (i=1; i<args.length; i++)
          this.mFileInst.append(args[i]);
      (args[0] || this.mPath)?rv=this.mPath = this.mFileInst.path:rv=null;
    } else {
      this.mFileInst = new JS_FS_File_Path(args?args:this.mPath);
      this.mFileInst.path?rv=this.mPath = this.mFileInst.path:rv=null;
    }
  } catch(e) {
    jslibError(e?e:null, 
      "initPath (nsILocalFile problem)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":initPath");
    rv = null;
  }
  return rv;
},

/***************************
*  CHECK INST              *
***************************/
checkInst : function () 
{

  if (!this.mFileInst) {
    jslibError(null, 
      "(no path defined)", 
      "NS_ERROR_NOT_INITIALIZED", 
      JS_FILESYSTEM_FILE+":checkInstance");
    return false;
  }
  return true;
},

/***************************
*  PATH                    *
***************************/
get path() 
{ 

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  return this.mFileInst.path; 
},

/***************************
*  EXISTS                  *
***************************/
exists : function ()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = false;
  try { 
    rv = this.mFileInst.exists(); 
  } catch(e) {
    jslibError(e, 
      "exists (nsILocalFile problem)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":exists");
    rv = false;
  }
  return rv;
},

/***************************
*  GET LEAF                *
***************************/
get leaf()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = null;
  try { 
    rv = this.mFileInst.leafName; 
  } catch(e) {
    jslibError(e, 
      "(problem getting file instance)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":leaf");
    rv = null;
  }
  return rv;
},

/***************************
*  SET LEAF                *
***************************/
set leaf(aLeaf)
{

  if (!aLeaf) {
    jslibError(null, 
      "(missing argument)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":leaf");
    return null;
  }
  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = null;
  try { 
    rv = (this.mFileInst.leafName=aLeaf); 
  } catch(e) {
    jslibError(e, 
        "(problem getting file instance)", 
        "NS_ERROR_FAILURE", 
        JS_FILESYSTEM_FILE+":leaf");
    rv = null;
  }
  return rv;
},

/***************************
*  PARENT                  *
***************************/
get parent()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = null;
  try { 
    if (this.mFileInst.parent.isDirectory()) {
      if (typeof(JS_DIR_LOADED)!='boolean')
        include(JS_LIB_PATH+'io/dir.js');
      rv = new Dir(this.mFileInst.parent.path);
    }
  } catch (e) {
    jslibError(e, 
      "(problem getting file parent)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":parent");
    rv = null;
  }
  return rv;
},

/***************************
*  GET PERMISSIONS         *
***************************/
get permissions()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":permisions");
    return null;
  }
  var rv = null;
  try { 
    rv = this.mFileInst.permissions.toString(8); 
  } catch(e) {
    jslibError(e, 
                "(problem getting file instance)", 
                "NS_ERROR_UNEXPECTED", 
                JS_FILESYSTEM_FILE+":permissions");
    rv = null;
  }
  return rv;
},

/***************************
*  SET PERMISSIONS         *
***************************/
set permissions(aPermission)
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;

  if (!aPermission) {
    jslibError(null, 
      "(no new permission defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":permissions");
    return null;
  }
  if (!this.exists()) {
    jslibError(null, 
                "(file doesn't exist)", 
                "NS_ERROR_FAILURE", 
                JS_FILESYSTEM_FILE+":permisions");
    return null;
  }
  if (!this.validatePermissions(aPermission)) {
    jslibError(null, 
                "(invalid permission argument)", 
                "NS_ERROR_INVALID_ARG", 
                JS_FILESYSTEM_FILE+":permissions");
    return null;
  }
  var rv = null;
  try { 
    rv = this.mFileInst.permissions=aPermission; 
  } catch(e) {
    jslibError(e, 
      "(problem getting file instance)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":permissions");
    rv = null;
  }
  return rv;

},

/***************************
*  VALIDATE PERMISSIONS    *
***************************/
validatePermissions : function (aNum)
{
  if (typeof(aNum)!='number')
    return false;
  if (parseInt(aNum.toString(10).length) < 3 )
    return false;
  return true;
},

/***************************
*  MODIFIED                *
***************************/
get dateModified()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":dateModified");
    return null;
  }
  var rv = null;
  try { 
    rv = (new Date(this.mFileInst.lastModifiedTime)); 
  } catch(e) {
    jslibError(e, 
      "(problem getting file instance)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":dateModified");
    rv = null;
  }
  return rv;
},

/***************************
*  RESET CACHE             *
***************************/
resetCache : function()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = false;
  if (this.mPath) {
    delete this.mFileInst;
    try {
      this.mFileInst=new JS_FS_File_Path(this.mPath);
      rv = true;
    } catch(e) {
      jslibError(e, 
        "(unable to get nsILocalFile)", 
        "NS_ERROR_UNEXPECTED", 
        JS_FILESYSTEM_FILE+":resetCache");
      rv = false;
    }
  }
  return rv;
},

/***************************
*  nsIFILE                 *
***************************/
get nsIFile()
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  var rv = null;
  try { 
    rv = this.mFileInst.clone(); 
  } catch (e) {
    jslibError(e, 
      "(problem getting file instance)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":nsIFile");
    rv = null;
  }
  return rv;
},

/***************************
*  NOTE: after a move      *
*  successful, 'this' will *
*  be reinitialized        *
*  to the moved file!      *
***************************/
move : function (aDest)
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  if (!aDest) {
    jslibError(null, 
      "(no destination path defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":move");
    return false;
  }
  if (!this.mPath) {
    jslibError(null, 
      "(no path defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":move");
    return false;
  }
  var rv = null;
  var newName=null;
  try {
    var f = new JS_FS_File_Path(aDest);
    if (f.exists() && !f.isDirectory()) {
      jslibError(null, 
        "(destination file exists remove it)", 
        "NS_ERROR_INVALID_ARG", 
        JS_FILESYSTEM_FILE+":move");
      return false;
    }
    if (f.equals(this.mFileInst)) {
      jslibError(null, 
        "(destination file is this file)", 
        "NS_ERROR_INVALID_ARG", 
        JS_FILESYSTEM_FILE+":move");
      return false;
    }
    if (!f.exists() && f.parent.exists())
      newName=f.leafName;
    if (f.equals(this.mFileInst.parent) && !newName) {
      jslibError(null,  
        "(destination file is this file)", 
        "NS_ERROR_INVALID_ARG", 
        JS_FILESYSTEM_FILE+":move");
      return false;
    }
    var dir=f.parent;
    if (dir.exists() && dir.isDirectory()) {
      jslibDebug(newName);
      this.mFileInst.moveTo(dir, newName);
      jslibDebug(JS_FILESYSTEM_FILE+':move successful!\n');
      this.mPath=f.path;
      this.resetCache();
      delete dir;
      rv = true;
    } else {
      jslibError(null, 
        "(destination "+dir.parent.path+" doesn't exists)", 
        "NS_ERROR_INVALID_ARG", 
        JS_FILESYSTEM_FILE+":move");
      return false;
    }
  } catch (e) {
    jslibError(e, 
      "(problem getting file instance)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":move");
    rv = false;
  }
  return rv;
},

/***************************
*  APPEND                  *
***************************/
append : function(aLeaf)
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  if (!aLeaf) {
    jslibError(null, 
      "(no argument defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":append");
    return null;
  }
  if (!this.mPath) {
    jslibError(null, 
      "(no path defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":append");
    return null;
  }
  var rv = null;
  try {
    this.mFileInst.append(aLeaf);
    rv = this.mPath=this.path;
  } catch(e) {
    jslibError(null, 
      "(unexpected error)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":append");
    rv = null;
  }
  return rv;
},

/***************************
*  APPEND RELATIVE         *
***************************/
appendRelativePath : function(aRelPath)
{

  if (!this.checkInst())
    throw Components.results.NS_ERROR_NOT_INITIALIZED;
  if (!aRelPath) {
    jslibError(null, 
      "(no argument defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":appendRelativePath");
    return null;
  }
  if (!this.mPath) {
    jslibError(null, 
      "(no path defined)", 
      "NS_ERROR_INVALID_ARG", 
      JS_FILESYSTEM_FILE+":appendRelativePath");
    return null;
  }
  var rv = null;
  try {
    this.mFileInst.appendRelativePath(aRelPath);
    rv = this.mPath=this.path;
  } catch(e) {
    jslibError(null, 
      "(unexpected error)", 
      "NS_ERROR_UNEXPECTED", 
      JS_FILESYSTEM_FILE+":appendRelativePath");
    rv = null;
  }
  return rv;
},

/***************************
*  GET URL                 *
***************************/
get URL()
{
  return (this.path?'file://'+this.path.replace(/\ /g, "%20").replace(/\\/g, "\/"):'');
},

/***************************
*  ISDIR                   *
***************************/
isDir : function()
{

  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":isDir");
    return false;
  }
  return this.mFileInst.isDirectory();
},

/***************************
*  ISFILE                  *
***************************/
isFile : function()
{

  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE",     
      JS_FILESYSTEM_FILE+":isDir");
    return false;
  }
  return this.mFileInst.isFile();
},

/***************************
*  ISEXEC                  *  
***************************/
isExec : function()
{

  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":isDir");
    return false;
  }
  return this.mFileInst.isExecutable();
},

/***************************
*  ISSYMLINK               *
***************************/
isSymlink : function()
{

  if (!this.exists()) {
    jslibError(null, 
      "(file doesn't exist)", 
      "NS_ERROR_FAILURE", 
      JS_FILESYSTEM_FILE+":isDir");
    return false;
  }
  return this.mFileInst.isSymlink();
},

/***************************
*  HELP                    *
***************************/
help  : function()
{

  const help =
    "\n\nFunction and Attribute List:\n"    +
    "\n"                                    +
    "   initPath(aPath);\n"                 +
    "   path;\n"                            +
    "   exists();\n"                        +
    "   leaf;\n"                            +
    "   parent;\n"                          +
    "   permissions;\n"                     +
    "   dateModified;\n"                    +
    "   nsIFile;\n"                         +
    "   move(aDest);\n"                     +
    "   append(aLeaf);\n"                   +
    "   appendRelativePath(aRelPath);\n"    +
    "   URL;\n"                             +
    "   isDir();\n"                         +
    "   isFile();\n"                        +
    "   isExec();\n"                        +
    "   isSymlink();\n";
  return help;
} 

}; // END FileSystem Class

jslibDebug('*** load: '+JS_FILESYSTEM_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

else {
    dump("JS_FILE library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/io/filesystem.js');\n\n");
}
