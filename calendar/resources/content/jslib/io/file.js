/*** -*- Mode: Javascript; tab-width: 2; 
The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is jslib code.
The Initial Developer of the Original Code is jslib team.

Portions created by jslib team are
Copyright (C) 2000 jslib team.  All
Rights Reserved.

Contributor(s): Pete Collins, 
                Doug Turner, 
                Brendan Eich, 
                Warren Harris, 
                Eric Plaster,
                Martin Kutschker

The purpose of this file is to make it a little easier to use 
xpcom nsIFile file IO library from js

 File API 
    file.js

 Base Class:
    FileSystem
      filesystem.js

 Function List:
    // Constructor
    File(aPath)                         creates the File object and sets the file path

    // file stream methods
    open(aMode, aPermissions);          open a file handle for reading,
                                        writing or appending.  permissions are optional.
    read();                             returns the contents of a file
    readline();                         returns the next line in the file.
    EOF;                                boolean check 'end of file' status
    write(aContents);                   writes the contents out to file.
    copy(aDest);                        copy the current file to a aDest
    close();                            closes a file handle
    create();                           creates a new file if one doesn't already exist
    exists();                           check to see if a file exists

    // file attributes
    size;                               read only attribute gets the file size
    ext;                                read only attribute gets a file extension if there is one
    permissions;                        attribute gets or sets the files permissions
    dateModified;                       read only attribute gets last modified date in locale string

    // file path attributes
    leaf;                               read only attribute gets the file leaf
    path;                               read only attribute gets the path
    parent;                             read only attribute gets parent dir part of a path

    // direct manipulation
    nsIFIle                             returns an nsIFile obj

    // utils
    remove();                           removes the current file
    append(aLeaf);                      appends a leaf name to the current file
    appendRelativePath(aRelPath);       appends a relitave path the the current file

    // help!
    help;                               currently dumps a list of available functions

 Instructions:

       First include this js file in your xul file.  
       Next, create an File object:

          var file = new File("/path/file.ext");

       To see if the file exists, call the exists() member.  
       This is a good check before going into some
       deep code to try and extract information from a non-existant file.

       To open a file for reading<"r">, writing<"w"> or appending<"a">,
       just call:

          file.open("w", 0644);

       where in this case you will be creating a new file called '/path/file.ext', 
       with a mode of "w" which means you want to write a new file.

       If you want to read from a file, just call:

          file.open(); or
          file.open("r");
          var theFilesContents    = file.read();

          ---- or ----

          while(!file.EOF) {
            var theFileContentsLine = file.readline();
            dump("line: "+theFileContentsLine+"\n");
          }

       The file contents will be returned to the caller so you can do something usefull with it.

          file.close();

       Calling 'close()' destroys any created objects.  If you forget to use file.close() no probs
       all objects are discarded anyway.

       Warning: these API's are not for religious types

************/

// insure jslib base is loaded
if (typeof(JS_LIB_LOADED)=='boolean') {

// test to make sure filesystem base class is loaded
if (typeof(JS_FILESYSTEM_LOADED)!='boolean')
  include(jslib_filesystem);

/****************** Globals **********************/
const JS_FILE_LOADED           = true;
const JS_FILE_FILE             = "file.js";

const JS_FILE_F_CHANNEL_CID    = "@mozilla.org/network/local-file-channel;1";
const JS_FILE_IOSERVICE_CID    = "@mozilla.org/network/io-service;1";
const JS_FILE_I_STREAM_CID     = "@mozilla.org/scriptableinputstream;1";
const JS_FILE_OUTSTREAM_CID    = "@mozilla.org/network/file-output-stream;1";

const JS_FILE_F_TRANSPORT_SERVICE_CID  = "@mozilla.org/network/file-transport-service;1";

const JS_FILE_I_FILE_CHANNEL           = "nsIFileChannel";
const JS_FILE_I_IOSERVICE              = C.interfaces.nsIIOService;
const JS_FILE_I_SCRIPTABLE_IN_STREAM   = "nsIScriptableInputStream";
const JS_FILE_I_FILE_OUT_STREAM        = C.interfaces.nsIFileOutputStream;

const JS_FILE_READ          = 0x01;  // 1
const JS_FILE_WRITE         = 0x08;  // 8
const JS_FILE_APPEND        = 0x10;  // 16

const JS_FILE_READ_MODE     = "r";
const JS_FILE_WRITE_MODE    = "w";
const JS_FILE_APPEND_MODE   = "a";

const JS_FILE_FILE_TYPE     = 0x00;  // 0

const JS_FILE_CHUNK         = 1024;  // buffer for readline => set to 1k

const JS_FILE_DEFAULT_PERMS = 0644;

const JS_FILE_OK            = true;

try {
  const JS_FILE_FileChannel  = new C.Constructor
  (JS_FILE_F_CHANNEL_CID, JS_FILE_I_FILE_CHANNEL);
  
  const JS_FILE_InputStream  = new C.Constructor
  (JS_FILE_I_STREAM_CID, JS_FILE_I_SCRIPTABLE_IN_STREAM);

  const JS_FILE_IOSERVICE    = C.classes[JS_FILE_IOSERVICE_CID].
  getService(JS_FILE_I_IOSERVICE);
} catch (e) {
  jslibError (e, "open("+this.mMode+") (unable to get nsIFileChannel)", 
                "NS_ERROR_FAILURE", 
                JS_FILE_FILE);
}

/***
 * Possible values for the ioFlags parameter 
 * From: 
 * http://lxr.mozilla.org/seamonkey/source/nsprpub/pr/include/prio.h#601
 */


// #define PR_RDONLY       0x01
// #define PR_WRONLY       0x02
// #define PR_RDWR         0x04
// #define PR_CREATE_FILE  0x08
// #define PR_APPEND       0x10
// #define PR_TRUNCATE     0x20
// #define PR_SYNC         0x40
// #define PR_EXCL         0x80

const JS_FILE_NS_RDONLY               = 0x01;
const JS_FILE_NS_WRONLY               = 0x02;
const JS_FILE_NS_RDWR                 = 0x04;
const JS_FILE_NS_CREATE_FILE          = 0x08;
const JS_FILE_NS_APPEND               = 0x10;
const JS_FILE_NS_TRUNCATE             = 0x20;
const JS_FILE_NS_SYNC                 = 0x40;
const JS_FILE_NS_EXCL                 = 0x80;
/****************** Globals **********************/


/****************************************************************
* void File(aPath)                                              *
*                                                               *
* class constructor                                             *
* aPath is an argument of string local file path                *
* returns NS_OK on success, exception upon failure              *
*   Ex:                                                         *
*     var p = '/tmp/foo.dat';                                   *
*     var f = new File(p);                                      *
*                                                               *
*   outputs: void(null)                                         *
****************************************************************/

function File(aPath) {

  if (!aPath) {
    jslibError(null,
              "Please enter a local file path to initialize",
              "NS_ERROR_XPC_NOT_ENOUGH_ARGS", JS_FILE_FILE);
    throw - C.results.NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }
  return this.initPath(arguments);
} // constructor

File.prototype = new FileSystem();

// member vars
File.prototype.mMode        = null;
File.prototype.mFileChannel = null;
File.prototype.mTransport   = null;
File.prototype.mURI         = null;
File.prototype.mOutStream   = null;
File.prototype.mInputStream = null;
File.prototype.mLineBuffer  = null;
File.prototype.mPosition    = 0;

/********************* OPEN *************************************
* bool open(aMode, aPerms)                                      *
*                                                               *
* opens a file handle to read, write or append                  *
* aMode is an argument of string 'w', 'a', 'r'                  *
* returns true on success, null on failure                      *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*                                                               *
*   outputs: void(null)                                         *
****************************************************************/

File.prototype.open = function(aMode, aPerms) 
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.mPath) {
    jslibError(null, "open("+this.mMode+") (no file path defined)", 
                      "NS_ERROR_NOT_INITIALIZED", 
                      JS_FILE_FILE+":open");
    return null;
  }

  if (this.exists() && this.mFileInst.isDirectory()) {
      jslibError(null, "open("+this.mMode+") (cannot open directory)", 
                        "NS_ERROR_FAILURE", 
                        JS_FILE_FILE+":open");
      return null;
  }

  if (this.mMode) {
    jslibError(null, "open("+this.mMode+") (already open)", 
                      "NS_ERROR_NOT_INITIALIZED", 
                      JS_FILE_FILE+":open");
    this.close();
    return null;
  }

  this.close();

  if (!this.mURI) {
    if (!this.exists())
      this.create();
    this.mURI = JS_FILE_IOSERVICE.newFileURI(this.mFileInst);
  }

  if (!aMode)
    aMode=JS_FILE_READ_MODE;

  this.resetCache();
  var rv;

  switch(aMode) {
    case JS_FILE_WRITE_MODE: 
    case JS_FILE_APPEND_MODE: {
      try {
        if (!this.mFileChannel)
          this.mFileChannel = JS_FILE_IOSERVICE.newChannelFromURI(this.mURI);
      } catch(e)    {
        jslibError(e, "open("+this.mMode+") (unable to get nsIFileChannel)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":open");
        return null;
      }    
      if (aPerms) {
        if (!this.validatePermissions(aPerms)) {
          jslibError(null, "open("+this.mMode+") (invalid permissions)", 
                    "NS_ERROR_INVALID_ARG", 
                    JS_FILE_FILE+":open");
          return null;
        }               
      }
      if (!aPerms)
        aPerms=JS_FILE_DEFAULT_PERMS;
      // removing, i don't think we need this --pete
      //this.permissions = aPerms;
      try {
        var offSet=0;
        if (aMode == JS_FILE_WRITE_MODE) {
          this.mMode=JS_FILE_WRITE_MODE;
          // create a filestream                                   
          var fs = C.classes[JS_FILE_OUTSTREAM_CID].               
                   createInstance(JS_FILE_I_FILE_OUT_STREAM);      
                                                                   
          fs.init(this.mFileInst, JS_FILE_NS_TRUNCATE |            
                                  JS_FILE_NS_WRONLY, 00004, null); 
          this.mOutStream = fs;                                    
        } else {
          this.mMode=JS_FILE_APPEND_MODE;
          // create a filestream                                  
          var fs = C.classes[JS_FILE_OUTSTREAM_CID].              
                   createInstance(JS_FILE_I_FILE_OUT_STREAM);     
                                                                  
          fs.init(this.mFileInst, JS_FILE_NS_RDWR |               
                                  JS_FILE_NS_APPEND, 00004, null);
          this.mOutStream = fs;                                   
        }
      } catch(e) {
        jslibError(e, "open("+this.mMode+") (unable to get file stream)", 
                    "NS_ERROR_FAILURE", 
                    JS_FILE_FILE+":open");
        return null;
      }
      try {
        // Use the previously created file transport to open an output 
        // stream for writing to the file
        if (!this.mOutStream) {
          // this.mOutStream = this.mTransport.openOutputStream(offSet, -1, 0);
          // this.mOutStream = 
        }
      } catch(e) {
        jslibError(e, "open("+this.mMode+") (unable to get outputstream)", 
                  "NS_ERROR_FAILURE", 
                  JS_FILE_FILE+":open");
        this.close();
        return null;
      }

      rv = true;
      break;
    }

    case JS_FILE_READ_MODE: {
      if (!this.exists()) {
        jslibError(null, "open(r) (file doesn't exist)", 
                          "NS_ERROR_FAILURE", 
                          JS_FILE_FILE+":open");
        return null;
      }
      this.mMode=JS_FILE_READ_MODE;
      try {
        this.mFileChannel = JS_FILE_IOSERVICE.newChannelFromURI(this.mURI);
        this.mInputStream        = new JS_FILE_InputStream();    
        this.mInputStream.init(this.mFileChannel.open());
        this.mLineBuffer         = new Array();
        rv=true;
      } catch (e) {
        jslibError(e, "open(r) (error setting permissions)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":open\n"+e);
        return null;
      }
      break;
    }

    default:
      jslibError(null, "open (must supply either w,r, or a)", 
                        "NS_ERROR_INVALID_ARG", 
                        JS_FILE_FILE+":open");
    return null;
  }
  return rv;
}

/********************* READ *************************************
* string read()                                                 *
*                                                               *
* reads a file if the file is binary it will                    *
* return type ex: ELF                                           *
* takes no arguments needs an open read mode filehandle         *
* returns string on success, null on failure                    *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.open(p);                                                *
*     f.read();                                                 *
*                                                               *
*   outputs: <string contents of foo.dat>                       *
****************************************************************/

File.prototype.read = function(aSize) 
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (this.mMode != JS_FILE_READ_MODE) {
    jslibError(null, "(mode is write/append)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":read");
    this.close();
    return null;
  }
  var rv = null;
  try {
    if (!this.mFileInst || !this.mInputStream) {
      jslibError(null, "(no file instance or input stream) ", 
                        "NS_ERROR_NOT_INITIALIZED", 
                        JS_FILE_FILE+":read");
      return null;
    }
    rv = this.mInputStream.read(aSize != undefined ? aSize : this.mFileInst.fileSize);
    this.mInputStream.close();
  } catch (e) { 
    jslibError(e, "read (input stream read)", 
                  "NS_ERROR_FAILURE", 
                  JS_FILE_FILE+":read");
    return null;
  }
  return rv;
}

/********************* READLINE**********************************
* string readline()                                             *
*                                                               *
* reads a file if the file is binary it will                    *
* return type string                                            *
* takes no arguments needs an open read mode filehandle         *
* returns string on success, null on failure                    *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.open();                                                 *
*     while(!f.EOF)                                             *
*       dump("line: "+f.readline()+"\n");                       *
*                                                               *
*   outputs: <string line of foo.dat>                           *
****************************************************************/

File.prototype.readline = function()
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.mInputStream) {
    jslibError(null, "(no input stream)", 
                      "NS_ERROR_NOT_INITIALIZED", 
                      JS_FILE_FILE+":readline");
    return null;
  }
  var rv      = null;
  var buf     = null;
  var tmp     = null;
  try {
    if (this.mLineBuffer.length < 2) {
      buf               = this.mInputStream.read(JS_FILE_CHUNK);
      this.mPosition    = this.mPosition + JS_FILE_CHUNK;
      if (this.mPosition > this.mFileInst.fileSize) 
        this.mPosition  = this.mFileInst.fileSize;
      if (buf) {
        if (this.mLineBuffer.length == 1) {
          tmp = this.mLineBuffer.shift();
          buf = tmp+buf;
        }
        this.mLineBuffer = buf.split(/[\n\r]/);
      }
    }
    rv = this.mLineBuffer.shift();
  } catch (e) {
    jslibError(e, "(problems reading from file)", 
                  "NS_ERROR_NOT_INITIALIZED", 
                  JS_FILE_FILE+":readline");
    rv = null;
  }
  return rv;
}

/********************* EOF **************************************
* bool getter EOF()                                             *
*                                                               *
* boolean check 'end of file' status                            *
* return type boolean                                           *
* takes no arguments needs an open read mode filehandle         *
* returns true on eof, false when not at eof                    *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.open();                                                 *
*     while(!f.EOF)                                             *
*       dump("line: "+f.readline()+"\n");                       *
*                                                               *
*   outputs: true or false                                      *
****************************************************************/

File.prototype.__defineGetter__('EOF', 
function()
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.mInputStream) {
    jslibError(null, "(no input stream)", 
                      "NS_ERROR_NOT_INITIALIZED", 
                      JS_FILE_FILE+":EOF");
    throw C.results.NS_ERROR_NOT_INITIALIZED;
  }

  if ((this.mLineBuffer.length > 0) || (this.mInputStream.available() > 0)) 
    return false;
  
  else 
    return true;
  
})

/********************* WRITE ************************************
* bool write()                                                  *
*                                                               *
* reads a file if the file is binary it will                    *
* return type ex: ELF                                           *
* takes no arguments needs an open read mode filehandle         *
* returns string on success, null on failure                    *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.open(p);                                                *
*     f.read();                                                 *
*                                                               *
*   outputs: <string contents of foo.dat>                       *
****************************************************************/

File.prototype.write = function(aBuffer, aPerms)
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (this.mMode == JS_FILE_READ_MODE) {
    jslibError(null, "(in read mode)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":write");
    this.close();
    return null;
  }

  if (!this.mFileInst) {
    jslibError(null, "(no file instance)", 
                      "NS_ERROR_NOT_INITIALIZED", 
                      JS_FILE_FILE+":write");
    return null;
  }

  if (!aBuffer)
    throw(JS_FILE_FILE+":write:ERROR: must have a buffer to write!\n");

  var rv=null;

  try {
    this.mOutStream.write(aBuffer, aBuffer.length);
    this.mOutStream.flush();
    //this.mOutStream.close();
    rv=true;
  } catch (e) { 
    jslibError(e, "write (nsIOutputStream write/flush)", 
                  "NS_ERROR_UNEXPECTED", 
                  JS_FILE_FILE+":write");
    rv=false;
  }
  return rv;
}

/********************* COPY *************************************
* void copy(aDest)                                              *
*                                                               *
* void file close                                               *
* return type void(null)                                        *
* takes no arguments closes an open file stream and             *
* deletes member var instances of objects                       *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     fopen();                                                  *
*     f.close();                                                *
*                                                               *
*   outputs: void(null)                                         *
****************************************************************/

File.prototype.copy = function (aDest)
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!aDest) {
    jslibError(null, "(no dest defined)", 
                      "NS_ERROR_INVALID_ARG", 
                      JS_FILE_FILE+":copy");
    throw -C.results.NS_ERROR_INVALID_ARG;
  }

  if (!this.exists()) {
    jslibError(null, "(file doesn't exist)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":copy");
    throw -C.results.NS_ERROR_FAILURE;
  }

  var rv;
  try {
    var dest          = new JS_FS_File_Path(aDest);
    jslibDebug(dest);
    var copyName      = null;
    var dir           = null;

    if (dest.equals(this.mFileInst)) {
      jslibError(null, "(can't copy file to itself)", 
                        "NS_ERROR_FAILURE", 
                        JS_FILE_FILE+":copy");
      throw -C.results.NS_ERROR_FAILURE;
    }

    if (dest.exists()) {
      jslibError(null, "(dest "+dest.path+" already exists)", 
                              "NS_ERROR_FAILURE", 
                              JS_FILE_FILE+":copy");
      throw -C.results.NS_ERROR_FAILURE;
    }

    if (this.mFileInst.isDirectory()) {
      jslibError(null, "(cannot copy directory)", 
                        "NS_ERROR_FAILURE", 
                        JS_FILE_FILE+":copy");
      throw -C.results.NS_ERROR_FAILURE;
    }

    if (!dest.exists()) {
      copyName          = dest.leafName;
      dir               = dest.parent;

      if (!dir.exists()) {
        jslibError(null, "(dest "+dir.path+" doesn't exist)", 
                          "NS_ERROR_FAILURE", 
                          JS_FILE_FILE+":copy");
        throw -C.results.NS_ERROR_FAILURE;
      }

      if (!dir.isDirectory()) {
        jslibError(null, "(dest "+dir.path+" is not a valid path)", 
                          "NS_ERROR_FAILURE", 
                          JS_FILE_FILE+":copy");
        throw -C.results.NS_ERROR_FAILURE;
      }
    }

    if (!dir) {
      dir = dest;
      if (dest.equals(this.mFileInst)) {
        jslibError(null, "(can't copy file to itself)", "NS_ERROR_FAILURE", JS_FILE_FILE+":copy");
        throw -C.results.NS_ERROR_FAILURE;
      }
    }
    this.mFileInst.copyTo(dir, copyName);
    jslibDebug(JS_FILE_FILE+":copy successful!");
  } catch (e) {
    jslibError(e, "(unexpected error)", "NS_ERROR_UNEXPECTED", JS_FILE_FILE+":copy");
  }
  return;
}

/********************* CLOSE ************************************
* void close()                                                  *
*                                                               *
* void file close                                               *
* return type void(null)                                        *
* takes no arguments closes an open file stream and             *
* deletes member var instances of objects                       *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     fopen();                                                  *
*     f.close();                                                *
*                                                               *
*   outputs: void(null)                                         *
****************************************************************/

File.prototype.close = function() 
{
  /***************** Destroy Instances *********************/
  if (this.mFileChannel)   delete this.mFileChannel;
  if (this.mInputStream)   delete this.mInputStream;
  if (this.mTransport)     delete this.mTransport;
  if (this.mMode)          this.mMode=null;
  if (this.mOutStream) {
    this.mOutStream.close();
    delete this.mOutStream;
  }
  if (this.mLineBuffer)    this.mLineBuffer=null;
  this.mPosition           = 0;
  /***************** Destroy Instances *********************/

  return void(null);
}

/********************* CREATE *****************************/
File.prototype.create = function()
{

  // We can probably implement this so that it can create a 
  // file or dir if a long non-existent mPath is present

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (this.exists()) {
    jslibError(null, "(file already exists)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":create");
    return null
  }

  if (!this.mFileInst.parent.exists() && this.mFileInst.parent.isDirectory()) {
    jslibError(null, "(no such file or dir: '"+this.path+"' )", 
                      "NS_ERROR_INVALID_ARG", 
                      JS_FILE_FILE+":create");
    return null
  }

  var rv=null;
  try { 
    rv = this.mFileInst.create(JS_FILE_FILE_TYPE, JS_FILE_DEFAULT_PERMS); 
  } catch (e) {
    jslibError(e, "(unexpected)", 
                  "NS_ERROR_UNEXPECTED",  
                  JS_FILE_FILE+":create");
    rv=null;
  }
  return rv;
}

/********************* REMOVE *******************************/
File.prototype.remove = function ()
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.mPath) {
    jslibError(null, "(no path defined)", 
                      "NS_ERROR_INVALID_ARG", 
                      JS_FILE_FILE+":remove");
    return null;
  }
  this.close();
  var rv;
  try {
    if (!this.exists()) {
      jslibError(null, "(file doesn't exist)", 
                        "NS_ERROR_FAILURE", 
                        JS_FILE_FILE+":remove");
      return null;
    }

    // I think we should remove whatever is there, sym link, file, dir etc . .
    if (this.mFileInst.isSpecial()) {
      jslibError(null, "(file is not a regular file)", 
                        "NS_ERROR_FAILURE", 
                        JS_FILE_FILE+":remove");
      return null;
    }

    if (!this.isFile()) {
      jslibError(null, "(not a file)", "NS_ERROR_FAILURE", JS_FILE_FILE+":remove");
      return null;
    }
    // this is a recursive remove.
    this.isFile() ? rv = this.mFileInst.remove(true) : rv = null; 
  } catch (e) {
    jslibError(e, "(unexpected)", 
                  "NS_ERROR_UNEXPECTED", 
                  JS_FILE_FILE+":remove");
    rv=null;
  }
  return rv;
}

/********************* POS **************************************
* int getter POS()                                              *
*                                                               *
* int file position                                             *
* return type int                                               *
* takes no arguments needs an open read mode filehandle         *
* returns current position, default is 0 set when               *
* close is called                                               *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.open();                                                 *
*     while(!f.EOF){                                            *
*       dump("pos: "+f.pos+"\n");                               *
*       dump("line: "+f.readline()+"\n");                       *
*     }                                                         *
*                                                               *
*   outputs: int pos                                            *
****************************************************************/

File.prototype.__defineGetter__('pos', function(){ return this.mPosition; })

/********************* SIZE *************************************
* int getter size()                                             *
*                                                               *
* int file size                                                 *
* return type int                                               *
* takes no arguments a getter only                              *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.size;                                                   *
*                                                               *
*   outputs: int 16                                             *
****************************************************************/

File.prototype.__defineGetter__('size',
function()
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.mPath) {
    jslibError(null, "size (no path defined)", 
                      "NS_ERROR_INVALID_ARG", 
                      JS_FILE_FILE);
    return null;
  }

  if (!this.exists()) {
    jslibError(null, "size (file doesn't exist)", "NS_ERROR_FAILURE", JS_FILE_FILE);
    return null;
  }
  var rv=null;
  this.resetCache();
  try { 
    rv=this.mFileInst.fileSize; 
  } catch(e) {
    jslibError(e, "(problem getting file instance)", "NS_ERROR_UNEXPECTED", JS_FILE_FILE+":size");
    rv=null;
  }

  return rv;
}) //END size Getter

/********************* EXTENSION ********************************
* string getter ext()                                           *
*                                                               *
* string file extension                                         *
* return type string                                            *
* takes no arguments a getter only                              *
*   Ex:                                                         *
*     var p='/tmp/foo.dat';                                     *
*     var f=new File(p);                                        *
*     f.ext;                                                    *
*                                                               *
*   outputs: dat                                                *
****************************************************************/

File.prototype.__defineGetter__('ext', 
function()
{

  if (!this.checkInst())
    throw C.results.NS_ERROR_NOT_INITIALIZED;

  if (!this.exists()) {
    jslibError(null, "(file doesn't exist)", 
                      "NS_ERROR_FAILURE", 
                      JS_FILE_FILE+":ext");
    return null;
  }
  if (!this.mPath) {
    jslibError(null, "(no path defined)", 
                      "NS_ERROR_INVALID_ARG", 
                      JS_FILE_FILE+":ext");
    return null;
  }
  var rv=null;
  try {
    var leafName  = this.mFileInst.leafName;
    var dotIndex  = leafName.lastIndexOf('.');
    rv=(dotIndex >= 0) ? leafName.substring(dotIndex+1) : "";
  } catch(e) {
    jslibError(e, "(problem getting file instance)", 
                  "NS_ERROR_UNEXPECTED", 
                  JS_FILE_FILE+":ext");
    rv=null;
  }
  return rv;
})// END ext Getter

File.prototype.super_help = FileSystem.prototype.help;

/********************* HELP *****************************/
File.prototype.__defineGetter__('help', 
function()
{
  const help = this.super_help()            +

    "   open(aMode);\n"                     +
    "   read();\n"                          +
    "   readline();\n"                      +
    "   EOF;\n"                             +
    "   write(aContents, aPermissions);\n"  +
    "   copy(aDest);\n"                     +
    "   close();\n"                         +
    "   create();\n"                        +
    "   remove();\n"                        +
    "   size;\n"                            +
    "   ext;\n"                             +
    "   help;\n";

  return help;
})

jslibDebug('*** load: '+JS_FILE_FILE+' OK');

} // END BLOCK JS_LIB_LOADED CHECK

// If jslib base library is not loaded, dump this error.
else
{
    dump("JS_FILE library not loaded:\n"                                +
         " \tTo load use: chrome://jslib/content/jslib.js\n"            +
         " \tThen: include('chrome://jslib/content/io/file.js');\n\n");
}
