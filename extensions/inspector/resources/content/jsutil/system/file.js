/*** -*- Mode: Javascript; tab-width: 2;

 ***** BEGIN LICENSE BLOCK *****
 Version: MPL 1.1/GPL 2.0/LGPL 2.1

 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the
 License.

 The Original Code is Collabnet code. The Initial Developer of the Original Code is Collabnet.

 The Initial Developer of the Original Code is
 Collabnet.
 Portions created by the Initial Developer are Copyright (C) 2000
 the Initial Developer. All Rights Reserved.

 Contributor(s):
   Pete Collins, Doug Turner, Brendan Eich, Warren Harris, Eric Plaster

 Alternatively, the contents of this file may be used under the terms of
 either the GNU General Public License Version 2 or later (the "GPL"), or
 the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.

 ***** END LICENSE BLOCK ***** */

/****************** Globals **********************/

const JSLIB_FILE                      = "file.js";

const JSFILE_LOCAL_CID                = "@mozilla.org/file/local;1";
const JSFILE_F_CHANNEL_CID            = "@mozilla.org/network/file-input-stream;1";
const JSFILE_I_STREAM_CID             = "@mozilla.org/scriptableinputstream;1";

const JSFILE_I_LOCAL_FILE             = "nsILocalFile";
const JSFILE_I_FILE_CHANNEL           = "nsIFileChannel";
const JSFILE_I_SCRIPTABLE_IN_STREAM   = "nsIScriptableInputStream";

const JSFILE_INIT_W_PATH              = "initWithPath";

const JSFILE_READ                     = 0x01;     // 1
const JSFILE_WRITE                    = 0x08;     // 8
const JSFILE_APPEND                   = 0x10;     // 16

const JSFILE_READ_MODE                = "r";
const JSFILE_WRITE_MODE               = "w";
const JSFILE_APPEND_MODE              = "a";

const JSFILE_FILETYPE                 = 0x00;     // 0

const JSFILE_CHUNK                    = 1024;     // buffer for readline => set to 1k

const JSFILE_OK                       = true;

const JSFILE_FilePath     = new Components.Constructor( JSFILE_LOCAL_CID, JSFILE_I_LOCAL_FILE, JSFILE_INIT_W_PATH);
const JSFILE_FileChannel  = new Components.Constructor( JSFILE_F_CHANNEL_CID, JSFILE_I_FILE_CHANNEL );
const JSFILE_InputStream  = new Components.Constructor( JSFILE_I_STREAM_CID, JSFILE_I_SCRIPTABLE_IN_STREAM );

/****************** Globals **********************/


/****************** File Object Class *********************/

function File(aPath) {
  if(aPath)
    this.mPath= aPath;
} // constructor

File.prototype  = {

  mPath           : null,
  mode            : null,
  fileInst        : null,
  fileChannel     : null,
  inputStream     : null,
  lineBuffer      : null,

/********************* OPEN *****************************/
open : function(aSetMode) 
{

  if(!this.mPath)
  {
    dump(JSLIB_FILE+":open:ERROR: Missing member path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }
 
  this.close();
  if(!aSetMode)
    aSetMode=JSFILE_READ_MODE;

  try { this.fileInst           = new JSFILE_FilePath(this.mPath); } 

  catch (e) 
  {
    dump(JSLIB_FILE+":open:ERROR: UNEXPECTED"+e.name+" "+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

var fileExists          = this.exists();

  if(fileExists) {
    if(this.fileInst.isDirectory())
    {
      dump(JSLIB_FILE+":open:ERROR: Can't open directory "+this.mPath+" as a file!\nNS_ERROR_NUMBER: ");
      return Components.results.NS_ERROR_FAILURE;
    }
  }

  var retval;
  switch(aSetMode)
  {
    case JSFILE_WRITE_MODE: 
    {
      if(fileExists)
      {
        try
        {
          this.fileInst.remove(false);
          fileExists=false;
        } 

        catch(e) 
        {
          dump(JSLIB_FILE+":open:ERROR: Failed to delete file "+this.mPath+"\nNS_ERROR_NUMBER: ");
          this.close();
          return Components.results.NS_ERROR_FAILURE;
        }
      }

      if (!fileExists)
      {
        try
        {
          this.fileInst.create(JSFILE_FILETYPE, 0644);
          fileExists=true;
        } 

        catch(e) 
        {
          dump(JSLIB_FILE+":open:ERROR: Failed to create file "+this.mPath+"\nNS_ERROR_NUMBER: ");
          this.close();
          return Components.results.NS_ERROR_FAILURE;
        }
      }

      this.mode=JSFILE_WRITE;
      retval=JSFILE_OK;
      break;
    }

    case JSFILE_APPEND_MODE:
    {
      if(!fileExists)
      {
        try
        {
          this.fileInst.create(JSFILE_FILETYPE, 0644);
          fileExists=true;
        } 

        catch(e) 
        {
          dump(JSLIB_FILE+":open:ERROR: Failed to create file "+this.mPath+"\nNS_ERROR_NUMBER: ");
          this.close();
          return Components.results.NS_ERROR_FAILURE;
        }
      }

      this.mode=JSFILE_APPEND;
      retval=JSFILE_OK;
      break;
    }

    case JSFILE_READ_MODE:
    {
      if(!fileExists)
      {
        dump(JSLIB_FILE+"open:ERROR: "+this.mPath+" doesn't exist!\nNS_ERROR_NUMBER: ");
        return Components.results.NS_ERROR_FAILURE
      }
    
      try {
        this.lineBuffer          = new Array();
        this.fileChannel         = new JSFILE_FileChannel();
        this.inputStream         = new JSFILE_InputStream();    

        try { var perm = this.fileInst.permissions; }        

        catch(e){ perm=null; }

        this.fileChannel.init(this.fileInst, JSFILE_READ, perm);
        this.inputStream.init(this.fileChannel.openInputStream());
        retval=JSFILE_OK;
      } 

      catch (e) 
      {
        dump(JSLIB_FILE+":open:ERROR: reading... "+e.name+" "+e.message+"\nNS_ERROR_NUMBER: ");
        return Components.results.NS_ERROR_FAILURE;
      }

      break;
    }

    default:
      dump(JSLIB_FILE+":open:WARNING: \""+setMode+"\" is an Invalid file mode\nNS_ERROR_NUMBER: ");
      return Components.results.NS_ERROR_INVALID_ARG;
  }

  return retval;
},

/********************* EXISTS ***************************/
exists : function () 
{
  var retval = null;

  try
  { 
    var file                = new JSFILE_FilePath(this.mPath);
    retval=file.exists();
  }

  catch(e) 
  { 
    dump(JSLIB_FILE+":exists:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: "); 
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* WRITE ****************************/
write : function(aBuffer, aPerms)
{
  if(!this.fileInst)
  {
    dump(JSLIB_FILE+":write:ERROR: Please open a file handle first\n NS_ERROR_NUMBER:");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  if(!aBuffer)
    throw(JSLIB_FILE+":write:ERROR: must have a buffer to write!\n");

  if(this.mode == JSFILE_READ)
    throw(JSLIB_FILE+":write:ERROR: not in write or append mode!\n");

  var buffSize            = aBuffer.length;
   
  try
  {
    if(!this.fileChannel)
      this.fileChannel      = new JSFILE_FileChannel();

    if(aPerms)
    {
      var checkPerms        = this.validatePermissions(aPerms);

      if(!checkPerms)
      {
        dump(JSLIB_FILE+":write:ERROR: invalid permissions set: "+aPerms+"\nNS_ERROR_NUMBER: ");
        return Components.results.NS_ERROR_INVALID_ARG
      }               
    }

    if(!aPerms)
      aPerms=0644;

    this.fileChannel.init(this.fileInst, this.mode, aPerms);
    try
    {
      this.fileInst.permissions=aPerms;
    }
    
    catch(e){ }    

    var outStream           = this.fileChannel.openOutputStream();

    outStream.write(aBuffer, buffSize);
    outStream.flush();
    outStream.close();
  }

  catch (e)
  { 
    dump(JSLIB_FILE+":write:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return JSFILE_OK;
},

/********************* READ *****************************/
read : function() 
{
  var retval = null;

  try 
  {
    if(!this.fileInst || !this.inputStream)
    {
      dump(JSLIB_FILE+":read:ERROR: Please open a valid file handle for reading\nNS_ERROR_NUMBER: ");
      return Components.results.NS_ERROR_NOT_INITIALIZED;
    }

    retval = this.inputStream.read(this.fileInst.fileSize);
    this.inputStream.close();
  }

  catch (e)
  { 
    dump(JSLIB_FILE+":read:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  return retval;
},

eof : function()
{
  if(!this.fileInst || !this.inputStream)
  {
    dump(JSLIB_FILE+":readline:ERROR: Please open a valid file handle for reading\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  if((this.lineBuffer.length > 0) || (this.inputStream.available() > 0)) 
    return false;
  
  else 
    return JSFILE_OK;
  
},

/********************* READLINE *****************************/
readline : function() 
{
  var retval  = null;
  var buf     = null;

  try 
  {
    if(!this.fileInst || !this.inputStream)
    {
      dump(JSLIB_FILE+":readline:ERROR: Please open a valid file handle for reading\nNS_ERROR_NUMBER: ");
      return Components.results.NS_ERROR_NOT_INITIALIZED;
    }

    if(this.lineBuffer.length < 2) {
      buf = this.inputStream.read(JSFILE_CHUNK);
      if(buf) {
        var tmp = null;
        if(this.lineBuffer.length == 1) {
          tmp = this.lineBuffer.shift();
          buf = tmp+buf;
        }
        this.lineBuffer = buf.split(/[\n\r]/);
      }
    }
    retval = this.lineBuffer.shift();
  }

  catch (e)
  { 
    dump(JSLIB_FILE+":readline:ERROR: "+e.name+" "+e.message+"\nNS_ERROR_NUMBER: ");
    throw(e);
  }

  return retval;
},

/********************* PATH *****************************/
path  : function () { return this.mPath; },
/********************* PATH *****************************/

/********************* LEAF *****************************/
leaf  : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":leaf:ERROR: Missing path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_INVALID_ARG;
  }

  if(!this.exists())
  {
    dump(JSLIB_FILE+":leaf:ERROR: File doesn't exist\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  var retval;
  try
  {
    var fileInst = new JSFILE_FilePath(this.mPath);
    retval=fileInst.leafName;
  }

  catch(e)
  { 
    dump(JSLIB_FILE+":leaf:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  return retval;
},

/********************* VALIDATE PERMISSIONS *************/
validatePermissions : function(aNum) 
{
  if ( parseInt(aNum.toString(10).length) < 3 ) 
    return false;

  return JSFILE_OK;
},

/********************* PERMISSIONS **********************/
permissions : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":permissions:ERROR: Missing path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  if(!this.exists())
  {
    dump(JSLIB_FILE+":permissions:ERROR: File doesn't exist\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  var retval;
  try
  { 
    var fileInst  = new JSFILE_FilePath(this.mPath); 
    retval=fileInst.permissions.toString(8);
  }

  catch(e)
  { 
    dump(JSLIB_FILE+":permissions:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* MODIFIED *************************/
dateModified  : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":dateModified:ERROR: Missing path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  if(!this.exists())
  {
    dump(JSLIB_FILE+":dateModified:ERROR: File doesn't exist\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  var retval;
  try
  { 
    var fileInst  = new JSFILE_FilePath(this.mPath); 
    var date = new Date(fileInst.lastModifiedTime).toLocaleString();
    retval=date;
  }

  catch(e)
  { 
    dump(JSLIB_FILE+":dateModified:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* SIZE *****************************/
size  : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":size:ERROR: Missing member path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  if(!this.exists())
  {
    dump(JSLIB_FILE+":size:ERROR: File doesn't exist\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  var retval;
  try
  { 
    var fileInst    = new JSFILE_FilePath(this.mPath); 
    retval          = fileInst.fileSize;
  }

  catch(e)
  { 
    dump(JSLIB_FILE+":size:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* EXTENSION ************************/
extension  : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":extension:ERROR: Missing member path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  if(!this.exists())
  {
    dump(JSLIB_FILE+":extension:ERROR: File doesn't exist\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_FAILURE;
  }

  var retval;
  try
  { 
    var fileInst  = new JSFILE_FilePath(this.mPath); 
    var leafName  = fileInst.leafName; 
    var dotIndex  = leafName.lastIndexOf('.'); 
    retval=(dotIndex >= 0) ? leafName.substring(dotIndex+1) : ""; 
  }

  catch(e)
  { 
    dump(JSLIB_FILE+":extension:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* DIRPATH **************************/
dirPath   : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":dirPath:ERROR: Missing member path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  var retval;
  try
  { 
    var fileInst            = new JSFILE_FilePath(this.mPath);

    if(fileInst.isFile())
      retval=fileInst.path.replace(this.leaf(), "");

    if(fileInst.isDirectory())
      retval=fileInst.path;
  }
  catch (e)
  { 
    dump(JSLIB_FILE+":dirPath:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* nsIFILE **************************/
nsIFile : function () 
{
  if(!this.mPath)
  {
    dump(JSLIB_FILE+":nsIFile:ERROR: Missing member path argument\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_NOT_INITIALIZED;
  }

  var retval;
  try
  {
    retval = new JSFILE_FilePath(this.mPath);
  }

  catch (e)
  { 
    dump(JSLIB_FILE+":nsIFile:ERROR: "+e.name+"\n"+e.message+"\nNS_ERROR_NUMBER: ");
    return Components.results.NS_ERROR_UNEXPECTED;
  }

  return retval;
},

/********************* HELP *****************************/
help  : function() 
{
  var help =

    "\n\nFunction List:\n"                  +
    "\n"                                    +
    "   open(aMode);\n"                     +
    "   exists();\n"                        +
    "   read();\n"                          +
    "   readline(aDone);\n"                 +
    "   write(aContents, aPermissions);\n"  +
    "   leaf();\n"                          +
    "   permissions();\n"                   +
    "   dateModified();\n"                  +
    "   size();\n"                          +
    "   extension();\n"                     +
    "   dirPath();\n"                       + 
    "   nsIFile();\n"                       + 
    "   help();\n";

  dump(help+"\n");

  return "";
},

/********************* CLOSE ****************************/
close : function() 
{
  /***************** Destroy Instances *********************/
  if(this.fileInst)       delete this.fileInst;
  if(this.fileChannel)    delete this.fileChannel;
  if(this.inputStream)    delete this.inputStream;
  if(this.mode)           this.mode=null;
  /***************** Destroy Instances *********************/

  return JSFILE_OK;
}
};

