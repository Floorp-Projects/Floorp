/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape Editor team, esp. Charles Manske <cmanske@netscape.com>
 *   ManyOne <http://www.manyone.net>
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

/* Classes to transfer a list of files and to track the transfer progress.
   It does not provide any UI.

   You basically only need to create a Transfer object and call its transfer()
   method. The callbacks will then be notified of the progress and possible
   errors. In the callbacks, you can ask about the status (incl. errors)
   of the files by reading the properties of the Transfer object
   (see Transfer.dump() and TransferFile.dump() for a short explanation).
   The functions in utility.js will help you with interpreting and
   displaying possible errors.

   The initial version/concept was stolen from Composer's publishing
   functionality, but heavily modified (probably almost all lines changed). */

const nsIChannel = Components.interfaces.nsIChannel;

  /*
    Passes parameters for transfer.

    @param download  Bool  download or upload
    @param serial  Bool  true: transfer one file after the other
                         false: all files at once (parallel)
    @param localDir  String  file:-URL of the local dir,
                               where the profile files are stored
    @param remoteDir  String   URL of the dir on the server,
                               where the profile files are stored
    @param password   String   Password for server in cleartext
    @param savepw     Bool     Should password be stored by def. when asked?
    @param files  Array of Objects with properties
                               filename  String  path relative to
                                                 localDir/remoteDir, plus
                                                 filename (incl. possible
                                                 extension)
                               mimetype  String  undefined, if unknown
                               size      int     in bytes.
                                                 undefined, if unknown
    @param progessCallback  function(file)  to be called when the progress
                               of a file transfer changed. can be used to e.g.
                               update the UI. parameter file is an entry in the
                               files array of this object, see makeFileVar()
                               and [this.]dump().
    @param finishedCallback  function(bool success)  to be called when
                               we're done. |success|, if it seems that
                               all files have been transferred successfully
                               and completely.
    @throws e.result==kMalformedURI  bad base URI
    @throws e.prop=="NoUsername"  no username given
   */
function Transfer(download, serial,
                  localDir, remoteDir,
                  password, savepw,
                  files,
                  finishedCallback, progressCallback)
{

  // from caller

  this.download = download;
  this.serial = serial;
  this.localDir = localDir;
  this.remoteDir = remoteDir;

  var uri = GetIOService().newURI(remoteDir, null, null);
             // this will throw an exception, if the user entered a bad URI.
             // catch in caller
  this.username = uri.username;
  if (!this.username || this.username == "")
    throw {prop:"NoUsername"};
  this.password = password;
  if (!savepw)
    savepw = false;
  this.savePassword = savepw; //savepw is when the user checked the checkbox
  this.saveLogin = false;     //savelogin is when we will need to write to disk
  this.sitename = uri.host;

  this.files = new Array();
  for (var i = 0, l = files.length; i < l; i++)
  {
    this.files[i] = new TransferFile(this, i,
                                     files[i].filename,
                                     files[i].mimetype,
                                     files[i].size);
  }


  // internal management

  this.cancelled = false; // user pressed cancel. Don't proceed.


  // progress

  this.progressSizeAll = 0;
  for (var i = 0, l = this.files.length; i < l; i++)
    this.progressSizeAll += this.files[i].size;
                    // We fixed up this.files[].size, so files != this.files
  this.progressSizeCompletedFiles = 0;
    /* Cumulated size of the files which have been *completely* (not partially)
       transferred. Used to speed up processing of incremental updates. */


  // callbacks

  this.finishedCallbacks = new Array();
  this.progressCallbacks = new Array();
  if (finishedCallback)
    this.finishedCallbacks.push(finishedCallback);
  if (progressCallback)
    this.progressCallbacks.push(progressCallback);
  /* I'm using arrays here, to allow the caller to hook up
     additional callbacks. */
}
Transfer.prototype =
{
  /* Called when all files finished
     @param  success  bool  no file failed
  */
  done : function(success)
  {
    //ddump("Done(" + (success ? "without" : "with") + " errors)");

    for (var i = 0, l = this.finishedCallbacks.length; i < l; i++)
      this.finishedCallbacks[i](success);

    /* The close dialog stuff (when all files are done successfully) is UI and
       thus in sroamingProgress.js, called from SetProgressStatus(), which
       should already be called. */
  },

  /* Initiate transfer of files
     Note: when this function returns, the transfer is most likely *not*
     finished yet.
     @throws "transfer failed" (from transferFile)
   */
  transfer : function()
  {
    //ddump("starting transfer");
    this.dump();

    if (this.files.length == 0)
      this.done(true);

    if (this.serial)
    {
      this.nextFile();
    }
    else
    {
      for (var i = 0, l = this.files.length; i < l; i++)
        this.transferFile(i);
    }
  },

  /* In serial mode, start the download of the next file, because all
     previous downloads are finished.
     @return  1: there are still files busy (something went wrong)
              2: transfer of next file started
              3: all files were already done, nothing left to do.
     @throws "transfer failed" (from transferFile)
   */
  nextFile : function()
  {
    // check that there are no downloads in progress
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      if (this.files[i].status == "busy")
      {
        dumpError("Programming error: nextFile() called, although there is "
                  + "still a download in progress.");
        this.dump();
        //return 1;
      }
    }

    // all halted?
    if (this.cancelled)
      return 3;

    // find the next pending one
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      if (this.files[i].status == "pending")
      {
        this.transferFile(i);
        return 2;
      }
    }

    return 3;
  },

  /* Transfer that file.
     Implements the actual upload/download.
     Independent of serial/parallel (caller is responsible for that).

     @param  id  index of this.files array of the file to be transferred
     @throws "transfer failed"
   */
  transferFile : function(id)
  {
    var file = this.files[id];
    //ddump("TransferFile(" + id + ")");

    try
    {
      var ioserv = GetIOService();
      var baseURL = ioserv.newURI(this.remoteDir, null, null);
      if (this.savePassword && this.password && this.password != "")
        baseURL.password = this.password;
      var localURL = ioserv.newURI(this.localDir, null, null);
      var channel = ioserv.newChannel(file.filename, null, baseURL);
      file.channel = channel;
      var progress = new TransferProgressListener(this, id);
      file.progressListener = progress;
      channel.notificationCallbacks = progress;
      //ddumpCont("Trying to "+ (this.download ? "download from" : "upload to"));
      //ddumpCont(" remote URL <" + channel.URI.spec + "> ");

      if (this.download)
      {
        var fos = Components
                   .classes["@mozilla.org/network/file-output-stream;1"]
                   .createInstance(Components.interfaces.nsIFileOutputStream);
        var bos = Components
                   .classes["@mozilla.org/network/buffered-output-stream;1"]
                   .createInstance(Components.interfaces
                                   .nsIBufferedOutputStream);
        var ssl = Components
                   .classes["@mozilla.org/network/simple-stream-listener;1"]
                   .createInstance(Components.interfaces
                                   .nsISimpleStreamListener);
        /* Download to temporary local location first, in case something
           goes wrong, e.g. failed auth (in that case, necko overwrites
           the file with nothing! :-( ). Moved to final location in
           fileFinished(). */
        var lf = ioserv.newURI(file.filename + ".tmp", null, localURL)
                       .QueryInterface(Components.interfaces.nsIFileURL)
                       .file.clone();
        file.localFile = lf;
        //ddump("to local file " + lf.path);
        try
        {
          fos.init(lf, -1, -1, 0);//odd params from NS_NewLocalFileOutputStream
        }
        catch(e)
        {
          var dir = lf.parent;
          if (e.result == kFileNotFound
               // we get this error, if the directory does no exist yet locally
              && dir && !dir.exists())
          {
            //ddumpCont("Creating dir " + dir.path + " ... ");
            dir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
                          // also creates all higher-level dirs, if necessary
            //ddump("done (hopefully).");
            fos.init(lf, -1, -1, 0); // try again
          }
          else
            throw e; // pass on all other errors
        }
        bos.init(fos, 32768);
        ssl.init(bos, progress);
        file.fos = fos; // for flush after finished
        file.bos = bos; // dito
        channel.asyncOpen(ssl, null);
      }
      else // upload
      {
        var uploadChannel = channel.QueryInterface(
                                      Components.interfaces.nsIUploadChannel);
        var lfURL = ioserv.newURI(file.filename, null, localURL);
        //ddump("from local file <" + lfURL.spec + ">");

        var fileChannel = ioserv.newChannelFromURI(lfURL);
        var is = fileChannel.open(); // maybe async? WFM.
        var bis = Components
                   .classes["@mozilla.org/network/buffered-input-stream;1"]
                   .createInstance(Components.interfaces
                                   .nsIBufferedInputStream);
        bis.init(is, 32768);
        uploadChannel.setUploadStream(bis, file.mimetype, -1);
        /* I *must* use a mimetype here. Passing null will cause the
           HTTP protocol implementation to use POST instead of PUT. */

        channel.asyncOpen(progress, null);
      }
    }
    catch(e)
    {
      if (e.result) // XPCOM error, we can report that to the user and we
             // partially even expect it (file not found in new profiles etc.)
      {
        //ddump("Transfer problem, will later report it to user:\n" + e);
        file.setStatus("failed", e.result, e.message);
      }
      else
      {
        //ddump("Unexpected (non-XPCOM) transfer problem:\n" + e);
        file.setStatus("failed", kErrorUnexpected, e);
      }
    }
  },

  // called when an individual file transfer completed or failed
  fileFinished : function(filenr)
  {
    //ddump("file " + filenr + " finished");

    var file = this.files[filenr];

    // flush/close - only for download
    if (file.bos && file.fos)
    {
      file.bos.flush();
      file.fos.flush();
      file.bos.close();
      file.fos.close();
    }

    // move to final location
    if (this.download && file.localFile && file.localFile.exists())
    {
      if (file.status == "done")
        file.localFile.moveTo(null, file.filename)
      else
        file.localFile.remove(false);
    }

    // check, if we're done with all files
    var done = true;
    var failed = false;
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      var file = this.files[i];
      if (file.status == "failed")
        failed = true; // |done| stays true
      else if (file.status != "done") // "failed" already tested above
        done = false;
    }
    if (done)
      this.done(!failed);

    else if (this.serial)
      this.nextFile();
  },

  // user wishes transfer to stop
  cancel : function()
  {
    //ddump("Canceling transfer");
    this.cancelled = true;  /* needed so that file.setStatus doesn't start
                               a next transfer in serial mode */

    // network stuff
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      if (this.files[i].status != "done" &&
          this.files[i].status != "failed")
      {
        this.files[i].setStatus("failed", kErrorAbort);
        var channel = this.files[i].channel;
        if (channel && channel.isPending)
        {
          channel.cancel(kErrorAbort);
          /* the above line sets all files in the UI to failed, but that does
             not close the dialog. */
        }
      }
    }

    /* finishedCallbacks called by done(), which will be called once all files
       are finished/cancelled. */
  },

  /* Hack.
     We need to change the list of files (add or remove files) after
     the Transfer object was created. However, in that case, the internal
     bookkeeping stuff (filenr of TransferFile) won't match anymore, so this
     function fixes that.
     Note: If you add files, create them using new TransferFile(). If you
     remove files, do that best using splice or readd the remaining files to a
     new array.
  */
  filesChanged : function()
  {
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      this.files[i].filenr = i;
    }
  },

  // Progress

  // functions dealing with tracking progress
  // not sure, if that should be at the progress listener

  progress : 0.0, // Gives the overall transfer progress of all files (0..1)
  progressCalculate : true, // if false, don't update this.progress

  /* The progress listener calls this function to notify it that the
     transfer progress of a certain file changed.
     This function will update the file's progress this.files[].progress
     and the overall progress this.progress.
     It will also call the owner's callbacks.

     SLOW! :-(

     @param filenr  int  index of the file whose progress changed
     @param aProgress  float 0..1  progress of that file
  */
  progressChanged : function(filenr, aProgress)
  {
    if (!this.progressCalculate)
      return;

    //ddump("Transfer.progressChanged(" + filenr + ", " + aProgress + ")");

    var file = this.files[filenr];
    file.progress = aProgress;

    if (this.progressSizeAll == 0) // undetermined mode
      return;

    // avoid too many updates
    if (file.progress - file.progressPrevious < 0.01)
      return;
    else
      file.progressPrevious = file.progress;


    // calculate progress
    var progressSize = 0;
    if (file.status == "done")
    {
      //ddump(" file completed mode");
      //ddump(" progressSizeAll: " + this.progressSizeAll);
      //ddumpCont(" progressSizeCompletedFiles before: ");
      //ddump(this.progressSizeCompletedFiles);

      this.progressSizeCompletedFiles = 0;
      for (var i = 0, l = this.files.length; i < l; i++)
      {
        var cur = this.files[i];
        if (cur.status == "done")
          this.progressSizeCompletedFiles += cur.size;
      }
      progressSize = this.progressSizeCompletedFiles;

      //ddumpCont(" progressSizeCompletedFiles now: ");
      //ddump(this.progressSizeCompletedFiles);
      //ddump(" progressSize: " + progressSize);
    }
    else
    {
      /* It's probably just an update during the file download -
         that happens often, so performance matters. */
      if (this.serial)
      {
        //ddump(" update serial mode");
        //ddump(" progressSizeAll: " + this.progressSizeAll);
        //ddump(" progressSizeCompletedFiles: "+this.progressSizeCompletedFiles);
        //ddump(" files[filenr].size: " + file.size);
        //ddump(" files[filenr].progress: " + file.progress);

        progressSize = this.progressSizeCompletedFiles
                       + file.size * file.progress;

        //ddump(" progressSize: " + progressSize);
      }
      else
      {
        //ddump(" update parallel mode");
        //ddump(" progressSizeCompletedFiles: "+this.progressSizeCompletedFiles);

        progressSize = this.progressSizeCompletedFiles;
        for (var i = 0, l = this.files.length; i < l; i++)
        {
          var cur = this.files[i];
          if (cur.status == "busy")
            progressSize += cur.size * cur.progress;
        }

        //ddump(" progressSize: " + progressSize);
      }
    }

    this.progress = progressSize / this.progressSizeAll;
       // I protected at the very beginning against this.progressSizeAll == 0

    for (var i = 0, l = this.progressCallbacks.length; i < l; i++)
      this.progressCallbacks[i](filenr);
  },

  
  // functions dealing with conflict-(resolution), i.e. ensuring that
  // we don't accidently overwrite newer files

  /* Queries the last-modification-time and size of the roaming files,
     from the OS and writes them all into a special local file.
     This file will also be uploaded and later be used to compare the ...
  */
  saveFileStats : function()
  {
  },


  // Pass back info to dialog/c++.

  /* returns int, really enum:
    0 = do nothing, ignore all password/login changes
    1 = Save password, set SavePW pref (user checked the box in the prompt)
    2 = Set SavePassword pref to false and ignore Password/Username retval
  */
  getSaveLogin : function()
  {
    if (this.saveLogin)
      return this.savePassword ? 1 : 2;
    else
      return 0;
  },

  getPassword : function()
  {
    if (this.getSaveLogin() == 1)
      return this.password;
    else
      return null;
  },

  getUsername : function()
  {
    if (this.getSaveLogin() == 1)
      return this.username;
    else
      return null;
  },


  // helpful functions for callers

  // returns an array of TransferFiles with all files with a certain status
  filesWithStatus : function(status)
  {
    var result = new Array();
    for (var i = 0, l = this.files.length; i < l; i++)
      if (this.files[i].status == status)
        result.push(this.files[i]);
    return result;
  },

  // utility/debug

  /*
    Prints to the console the contents of this object.
   */
  dump : function()
  {
    return;
    //ddump("Transfer object:");
    //ddump(" download: " + this.download);
        // bool, see constructor
    //ddump(" serial: " + this.serial);
        // bool, see constructor
    //ddump(" localDir: " + this.localDir);
        // String, see constructor
    //ddump(" remoteDir: " + this.remoteDir);
        // String, see constructor
    //ddump(" password: " + this.password);
        // String, see constructor
    //ddump(" savePassword: " + this.savePassword);
        // bool, see constructor
    //ddump(" sitename: " + this.sitename);
        // String, to be displayed to the user identifying the remote server

    //ddump(" progress: "+(this.progressCalculate ? this.progress : "disabled"));
        // float, 0..1, how much of the files (measured by filesizes) has
        // already been transfered
    //ddump(" progressSizeAll: " + this.progressSizeAll);
        // int, in bytes, cumulated filesizes as reported by the owner

    //ddump(" Files: (" + this.files.length + " files)");
    for (var i = 0, l = this.files.length; i < l; i++)
    {
      //ddump("  File " + i + ":");
      this.files[i].dump();
    }
    //ddump(" finished callbacks: " + this.finishedCallbacks.length);
    //ddump(" progress callbacks: " + this.progressCallbacks.length);
    //for (var i = 0, l = this.finishedCallbacks.length; i < l; i++)
      //dump(this.finishedCallbacks[i]);
  }
}


/* 
   @param transfer  Transfer  the owner
   @param filenr  int  index of this file in the transfer's files array

   @param filename, mimetype, size  see Transfer
*/
function TransferFile(transfer, filenr, // hooks to owner
                      filename, mimetype, size)
{
  // sanitize input
  if (mimetype == undefined || mimetype == "")
    mimetype = "application/octet-stream";
  if (size == undefined)
    size = 0;

  // for explanations, see this.dump()
  this.filename = filename;
  this.mimetype = mimetype;
  this.size = size;
  //ddump("got file " + filename + " with mimetype " + mimetype);
  this.status = "pending";
  this.statusCode = 0;
  this.progress = 0.0;
  this.channel = null;
  this.progressListener = null;
  this.fos = null;
  this.bos = null;
  this.localFile = null;

  this.transfer = transfer;
  this.filenr = filenr;
}
TransferFile.prototype =
{
  dump : function()
  {
    //ddump("   filename: " + this.filename);
      // String: relative pathname from profile root
    //ddump("   mimetype: " + this.mimetype);
      // String. might be undefined.
    //ddump("   size: " + this.size);
      // int, in bytes. might be 0 for undefined.
    //ddump("   status: " + this.status);
      // String: "pending" (not started), "busy", "done", "failed"
    //ddumpCont("   statusCode: " + this.statusCode + " (");
    //ddump(NameForStatusCode(this.statusCode) + ")");
      // int: nsresult error code that we got from Necko
    //ddump("   httpResponse: " + this.httpResponse);
      // int: HTTP response code that we got from the server
    //ddump("   statusText: " + this.statusText);
      // String: Text corresponding to error (usually with httpResponse
      // or fatal errors). Might be in English or the server's language.
      // Text (usually translated) corresponding to statusCode can be
      // gotten from ...utility.js; it may include this text.
    //ddump("   progress: " + this.progress);
      // float: 0 (nothing) .. 1 (complete)
  },

  /* Use this function to change this.status

     @param aStatus  String  like this.status, i.e. "done", "pending" etc.
     @param aStatusCode  Mozilla error code
     @param aMessage  String  Sets statusText, a long explanation specific
                              to this error case. Optional.
  */
  setStatus : function(aStatus, aStatusCode, aMessage)
  {
    //ddumpCont("request to change "+this.filename+" from "+this.status+" (");
    //ddumpCont(NameForStatusCode(this.statusCode)+") to "+aStatus+" (");
    var undef = aStatusCode == undefined;
    //ddump((undef ? "no new status code" : NameForStatusCode(aStatusCode))+")");

    if (this.statusCode == aStatusCode && this.status == aStatus)
    {
      //ddump("  (no changes)");
      return;
    }

    var was_failed = (this.status == "failed");
    var was_done = (this.status == "done");
    if (!was_failed)  /* prevent overwriting older errors with newer, bogus
                         OK status codes or the real problem with
                         consequential problems. */
    {
      this.status = aStatus;
      if (aStatusCode != undefined)
      {
        this.statusCode = aStatusCode;
        //this.statusText = aMessage; // might be undefined, but it must match
        //                            // this.statusCode
        // WORKAROUND *sigh*, for FTP, we get the error msg before the error
      }
      if (aMessage)
        this.statusText = aMessage;

      for (var i = 0, l = this.transfer.progressCallbacks.length; i < l; i++)
        this.transfer.progressCallbacks[i](this.filenr);
    }
    if (
        !(was_done || was_failed)
        && (aStatus == "done" || aStatus == "failed")
       )  // file just completed or failed
     this.transfer.fileFinished(this.filenr);
  }
}




/* Use one object per file to be downloaded.
   @param filenr  integer  index of file in transfer.files
   @param transfer  Transfer  the context
*/
function TransferProgressListener(transfer, filenr)
{
  this.transfer = transfer;  // this creates a cyclic reference :-(
  this.filenr = filenr;
  this.file = this.transfer.files[this.filenr];

        var consumer = Components.classes["@mozilla.org/binaryoutputstream;1"]
                       .createInstance(Components.interfaces
                                       .nsIBinaryOutputStream);
        /* I don't care about that stream, and it's empty anyways, so
           just use any outputstream that stores in RAM.
           nsStorageStream would probably have been more suited, but I don't
           see how I could instantiate that from JS, and binaryoutputstream
           works just as well. */
        var buffer = Components
                       .classes["@mozilla.org/network/buffered-output-stream;1"]
                       .createInstance(Components.interfaces
                                       .nsIBufferedOutputStream);
        this.dummy_ssl = Components
                   .classes["@mozilla.org/network/simple-stream-listener;1"]
                   .createInstance(Components.interfaces
                                   .nsISimpleStreamListener);
        buffer.init(consumer, 32768);
        this.dummy_ssl.init(buffer, null);
}
TransferProgressListener.prototype =
{

  // Progress

  // nsIRequestObserver

  onStartRequest : function(aRequest, aContext)
  {
    //ddump("onStartRequest: " + aRequest.name);
    this.file.setStatus("busy");
  },

  onStopRequest : function(aRequest, aContext, aStatusCode)
  {
    //ddump("onStopRequest:");
    //ddump("  Request: " + aRequest.name);
    //ddump("  StatusCode: " + NameForStatusCode(aStatusCode));

    if (aStatusCode == kNS_OK)
    {
      /* WORKAROUND
         HTTP gives us NS_OK, although the request failed with an HTTP error
         code, so check for that.
         FTP gives us NS_OK, although the transfer is still ongoing.
         I tried to work around that in onStatus(), see comment there. */
      var scheme = this.file.channel.URI.scheme;
      if (scheme == "http")
        this.privateHTTPResponse();
      //else if (scheme == "ftp")
      //  return;
      else
        // let's hope that the other protocol impl.s are saner
        this.file.setStatus("done", aStatusCode);
    }
    else
      this.privateStatusChange(aStatusCode);
  },

  // nsIProgressEventSink

  onStatus : function(aRequest, aContext, aStatusCode, aStatusArg)
  {
    /* WORKAROUND
       The status codes that are passed to us here in aStatusCode look like
       nsresult error codes, but their numerical values overlap with other,
       real errors. Darin said that real errors are never passed in here,
       so we don't have a hard conflict. To make processing easier, I just
       translate these status codes into made-up, unique error codes,
       so we can later check them together with other status and error codes
       and store them in the normal this.status field (which contains
       normal XPCOM error codes) etc.. */
    if      (aStatusCode == kStatusResolvingHost_Status)
              aStatusCode = kStatusResolvingHost;
    else if (aStatusCode == kStatusConnectedTo_Status)
              aStatusCode = kStatusConnectedTo;
    else if (aStatusCode == kStatusSendingTo_Status)
              aStatusCode = kStatusSendingTo;
    else if (aStatusCode == kStatusReceivingFrom_Status)
              aStatusCode = kStatusReceivingFrom;
    else if (aStatusCode == kStatusConnectingTo_Status)
              aStatusCode = kStatusConnectingTo;
    else if (aStatusCode == kStatusWaitingFor_Status)
              aStatusCode = kStatusWaitingFor;
    else if (aStatusCode == kStatusReadFrom_Status)
              aStatusCode = kStatusReadFrom;
    else if (aStatusCode == kStatusWroteTo_Status)
              aStatusCode = kStatusWroteTo;

    //ddump("onStatus:");
    //ddump("  Request: " + aRequest.name);
    //ddump("  StatusCode: " + NameForStatusCode(aStatusCode));
    //ddump("  StatusArg: " + aStatusArg);

    /* WORKAROUND
       We sometimes get onStopRequest *before* e.g. onStatus(SENDING_TO), so
       ignore all status msgs after onstop. This is dangerous, because
       IIRC I saw onStop also on the very beginning of a transfer, and
       if that happens for the last file, we might close the dialog before
       the transfer finished, but I don't know another quick and not too bad
       workaround. */
    if (this.file.status == "done")
      return;

    this.privateStatusChange(aStatusCode);
  },

  /* Use this function to interpret Mozilla status/error codes and
     update |this| appropriately.
     Don't use it for NS_OK - what this means depends on the situation and
     thus can't be interpreted here.
     @param aStatusCode  Mozilla error code
  */
  privateStatusChange : function(aStatusCode)
  {
    var status;
    if (aStatusCode == kNS_OK)
      // don't know what this means, only the caller knows what succeeded
      return;
    else if (aStatusCode == kStatusReadFrom)
      status = "busy";
    else if (aStatusCode == kStatusReceivingFrom)
      status = "busy";
    else if (aStatusCode == kStatusSendingTo)
      status = "busy";
    else if (aStatusCode == kStatusWaitingFor)
      status = "busy";
    else if (aStatusCode == kStatusResolvingHost)
      status = "busy";
    else if (aStatusCode == kStatusConnectedTo)
      status = "busy";
    else if (aStatusCode == kStatusConnectingTo)
      status = "busy";
    else if (aStatusCode == kErrorBindingRedirected)
      status = "busy";
    else if (aStatusCode == kErrorBindingRetargeted)
      status = "busy";
    else if (aStatusCode == kStatusBeginFTPTransaction)
      status = "busy";
    else if (aStatusCode == kStatusEndFTPTransaction)
      status = "done";
    else if (aStatusCode == kInProgress)
      status = "busy";
    else if (aStatusCode == kStatusFTPLogin)
      status = "busy";
    else if (aStatusCode == kErrorFTPAuthNeeded)
      status = "busy";
    else if (aStatusCode == kNetReset)
      ; // correct?
    else
      status = "failed";

    this.file.setStatus(status, aStatusCode);
  },

  /* Use this function to get and interpret HTTP status/error codes and
     update |this| appropriately.
  */
  privateHTTPResponse : function()
  {
    //ddumpCont("privateHTTPResponse ");

    // Get HTTP response code
    var responseCode;
    var respText;
    try
    {
      var httpchannel = this.file.channel
                        .QueryInterface(Components.interfaces.nsIHttpChannel);
      responseCode = httpchannel.responseStatus;
      this.file.httpResponse = responseCode;
      respText = httpchannel.responseStatusText;
      //ddump(responseCode);
    }
    catch(e)
    {
      //ddump("  Error while trying to get HTTP response: " + e);
      this.file.setStatus("failed", kErrorNoInterface);
      return;
    }

    // Interpret HTTP response code
    var status;
    if (responseCode >= 100 && responseCode < 200)
      status = "busy";
    else if (responseCode >= 200 && responseCode < 300)
      status = "done";
    else if (responseCode >= 300 && responseCode < 400)
      status = "busy";
    else if (responseCode >= 400 && responseCode < 600)
      status = "failed";
    else // what?
      return;

    this.file.setStatus(status, kStatusHTTP, respText);
  },

  onProgress : function(aRequest, aContext, aProgress, aProgressMax)
  {
    //ddump("onProgress: " + aProgress + "/" + aProgressMax);

    if (aProgressMax > 0 && aProgress > 0)
          // WORKAROUND Necko sometimes sends crap like 397/0 or 0/4294967295
    {
      this.transfer.progressChanged(this.filenr, aProgress / aProgressMax);
    }
  },


  // Dummies

  // nsStreamListener

  onDataAvailable : function(aRequest, aContext,
                             anInputStream, anOffset, aCount)
  {
    /* dummy, because it's only the down stream during an upload
       and during download, we use the tee, which copies to the outstream. */

    this.dummy_ssl.onDataAvailable(aRequest, aContext, anInputStream,
                                   anOffset, aCount);
  },

  // nsIFTPEventSink, nsIHTTPEventSink

  OnFTPControlLog : function(aServer, aMsg)
  {
    // dummy
  },

  OnRedirect : function(anHTTPChannel, aNewChannel)
  {
    // dummy
  },

  /*
  // nsIDocShellTreeItem (wtf?)
  name : "dummy in transfer.js",
  itemType : 0,
  parent : null,
  sameTypeParent : null,
  rootTreeItem : null,
  sameTypeRootTreeItem : null,
  treeOwner : null,
  childOffset : 0,
  nameEquals : function(name)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  findItemWithName : function(name, requestor)
  {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  */

  // Prompts

  // nsIPrompt
  alert : function(dlgTitle, text)
  {
    //ddump("alert " + text);

    /* WORKAROUND
       FTP sends us these in the case of an error *sigh*. Don't display dialog,
       but redirect to file errors. */
    this.file.statusText = text.replace(/\n/, "");

    /*
    if (!dlgTitle)
      title = GetString("Alert");
    GetPromptService().alert(window, dlgTitle, text);
    */
  },
  alertCheck : function(dlgTitle, text, checkBoxLabel, checkObj)
  {
    //ddump("alertCheck");
    this.alert(dlgTitle, text);
  },
  confirm : function(dlgTitle, text)
  {
    //ddump("confirm");
    return this.confirmCheck(dlgTitle, text, null, null);
  },
  confirmCheck : function(dlgTitle, text, checkBoxLabel, checkObj)
  {
    //ddump("confirmCheck");
    var promptServ = GetPromptService();
    if (!promptServ)
      return;

    promptServ.confirmEx(window, dlgTitle, text,
                         nsIPromptService.STD_OK_CANCEL_BUTTONS,
                         "", "", "", checkBoxLabel, checkObj);
  },
  confirmEx : function(dlgTitle, text, btnFlags,
                       btn0Title, btn1Title, btn2Title,
                       checkBoxLabel, checkVal)
  {
    //ddump("confirmEx");
    var promptServ = GetPromptService();
    if (!promptServ)
      return 0;

    return promptServ.confirmEx(window, dlgTitle, text, btnFlags,
                        btn0Title, btn1Title, btn2Title,
                        checkBoxLabel, checkVal);
  },
  select : function(dlgTitle, text, count, selectList, outSelection)
  {
    //ddump("select");
    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    return promptServ.select(window, dlgTitle, text, count,
                             selectList, outSelection);
  },
  prompt : function(dlgTitle, label, /*inout*/ inputvalueObj,
                    checkBoxLabel, /*inout*/ checkObj)
  {
    //ddump("nsIPrompt::prompt");
    return this.privatePrompt(dlgTitle, label, null, inputvalueObj,
                              checkBoxLabel, checkObj);
  },
  /* Warning: these |promptPassword| and |promptUsernameAndPassword|
     from nsIPrompt will never be invoked. If they are being called, the
     nsIAuthPrompt versions will be invoked, with wrong arguments.
     This is because we're overloading the functions with the same number
     of parameters and JS can't distinguish between different parameter types.
     This is a bug copied from Editor's Publishing feature; the bug exists
     there as well. */
  promptPassword : function(dlgTitle, label, /*inout*/ pwObj,
                            checkBoxLabel, /*inout*/ savePWObj)
  {
    //ddump("nsIPrompt::promptPassword:");
    //ddump("  pwObj: " + pwObj.value);
    //ddump("  checkBoxLabel: " + checkBoxLabel);
    //ddump("  savePWObj: " + savePWObj.value + " (ignored)");

    var ret = this.privatePromptPassword(dlgTitle, label, null,
                                         pwObj, checkBoxLabel);

    savePWObj.value = this.transfer.savePassword; // out param
    return ret;
  },
  promptUsernameAndPassword : function(dlgTitle, label,
                                       /*inout*/ userObj, /*inout*/ pwObj,
                                       savePWLabel, /*inout*/ savePWObj)
  {
    //ddump("nsIPrompt::promptUsernameAndPassword:");
    //ddump("  userObj: " + userObj.value);
    //ddump("  pwObj: " + pwObj.value);
    //ddump("  savePWLabel: " + savePWLabel);
    //ddump("  savePWObj: " + savePWObj.value + " (ignored)");

    var ret = this.privatePromptUsernameAndPassword(dlgTitle, label, null,
                                                    userObj, pwObj,
                                                    savePWLabel);

    savePWObj.value = this.transfer.savePassword; // out param
    return ret;
  },

  // nsIAuthPrompt

  prompt : function(dlgTitle, label, pwrealm, /*in*/ savePW, defaultText,
                    /*out*/ resultObj)
  {
    //ddump("nsIAuthPrompt::prompt");
    var savePWObj = {value:this.transfer.savePassword};
                    // I know it better than the caller.
    var inputvalueObj = {value:defaultText};
    var ret = this.privatePrompt(dlgTitle, label, pwrealm,
                                 inputvalueObj, null, savePWObj);
    resultObj.value = inputvalueObj.value;
    return ret;
  },
  promptPassword : function(dlgTitle, label, pwrealm, /*in*/ savePW,
                            /*out*/ pwObj)
  {
    //ddump("nsIAuthPrompt::promptPassword");
    //ddump("  pwrealm: " + pwrealm);
    //ddump("  savePW: " + savePW + " (ignored)");
    //ddump("  pwObj: " + pwObj.value);

    return this.privatePromptPassword(dlgTitle, label, pwrealm,
                                      pwObj, null);
  },
  promptUsernameAndPassword : function(dlgTitle, label, pwrealm, /*in*/savePW,
                                       /*out*/ userObj, /*out*/ pwObj)
  {
    //ddump("nsIAuthPrompt::promptUsernameAndPassword:");
    //ddump("  pwrealm: " + pwrealm);
    //ddump("  savePW: " + savePW + " (ignored)");
    //ddump("  userObj: " + userObj.value);
    //ddump("  pwObj: " + pwObj.value);

    return this.privatePromptUsernameAndPassword(dlgTitle, label, pwrealm,
                                                 userObj, pwObj, null);
  },

  // helper functions for prompt stuff

  privatePrompt : function(dlgTitle, label, pwrealm, /*inout*/ inputvalueObj,
                           checkBoxLabel, /*inout*/ checkObj)
  {
    if (!dlgTitle)
      dlgTitle = GetString("PasswordTitle");
    if (!pwrealm)
      pwrealm = this.transfer.sitename;
    if (!label)
      label = this.makePWLabel(userObj.value, pwrealm);
    if (!checkBoxLabel)
      checkBoxLabel = GetString("SavePassword");

    var promptServ = GetPromptService();
    if (!promptServ)
       return false;

    var ret = promptServ.prompt(window, dlgTitle, label,
                                inputvalueObj, checkBoxLabel, checkObj);
    if (!ret)
      this.transfer.cancel();
    return ret;
  },
  privatePromptPassword : function(dlgTitle, label, pwrealm,
                                   /*inout*/ pwObj, savePWLabel)
  {
    //ddump("privatePromptPassword()");
    //ddump("  pwObj.value: " + pwObj.value);
    //ddump("  savePWLabel: " + savePWLabel);
    //ddump("  label: " + label);
    //ddump("  pwrealm: " + pwrealm);
    //ddump("  this.transfer.savePassword: " + this.transfer.savePassword);
    //ddump("  this.transfer.username: " + this.transfer.username);

    if (!dlgTitle)
      dlgTitle = GetString("PasswordTitle");
    if (!pwrealm)
      pwrealm = this.transfer.sitename;
    if (!label)
      label = this.makePWLabel(userObj.value, pwrealm);
    if (!savePWLabel)
      savePWLabel = GetString("SavePassword");
    var savePWObj = {value: this.transfer.savePassword};

    //ddump("  pwObj.value: " + pwObj.value);
    //ddump("  savePWLabel: " + savePWLabel);
    //ddump("  label: " + label);
    //ddump("  pwrealm: " + pwrealm);

    var promptServ = GetPromptService();
    if (!promptServ)
      return false;

    var ret = false;
    try {
      ret = promptServ.promptPassword(window, dlgTitle, label,
                                      pwObj, savePWLabel, savePWObj);

      if (!ret)
        this.transfer.cancel();
      else
        this.updateUsernamePasswordFromPrompt(this.transfer.username,
                                              pwObj.value, savePWObj.value);
    } catch (e) {
      dumpError(e);
    }

    return ret;
  },
  privatePromptUsernameAndPassword : function(dlgTitle, label, pwrealm,
                                           /*inout*/ userObj, /*inout*/ pwObj,
                                           savePWLabel)
  {
    // HTTP prompts us twice even if user Cancels from 1st attempt!

    //ddump("privatePromptUsernameAndPassword()");
    //ddump("  pwObj.value: " + pwObj.value);
    //ddump("  userObj.value: " + userObj.value);
    //ddump("  savePWLabel: " + savePWLabel);
    //ddump("  label: " + label);
    //ddump("  pwrealm: " + pwrealm);
    //ddump("  this.transfer.savePassword: " + this.transfer.savePassword);
    //ddump("  this.transfer.username: " + this.transfer.username);

    if (!dlgTitle)
      dlgTitle = GetString("PasswordTitle");
    if (!pwrealm)
      pwrealm = this.transfer.sitename;
    if (!label)
      label = this.makePWLabel(userObj.value, pwrealm);
    if (!savePWLabel)
      savePWLabel = GetString("SavePassword");
    if (!userObj.value)
      // HTTP PUT uses this dialog if either username or password is bad,
      // so prefill username with the previous value
      userObj.value = this.transfer.username;
    var savePWObj = {value: this.transfer.savePassword};

    //ddump("  savePWObj.value: " + savePWObj.value);
    //ddump("  userObj.value: " + userObj.value);
    //ddump("  savePWLabel: " + savePWLabel);
    //ddump("  label: " + label);
    //ddump("  pwrealm: " + pwrealm);

    /* I had the idea to put up the password-only dialog, if we have a
       username. But if the user didn't enter a username in the prefs,
       it first all works correctly, the username+password dialog comes up.
       The problem starts when the user entered the wrong username -
       he won't have a chance to change it anymore. Not sure, if that is bad.
       So, the code for that is below.
       Update: I decided that the user has to enter a username in any case,
       and he can't change it during login. At least not in this case. */

    if (userObj.value && userObj.value != "")
    {
      // |label| here says "enter username and password ...", so we need
      // to build our own.
      label = this.makePWLabel(userObj.value, pwrealm);
      return this.privatePromptPassword(dlgTitle, label, pwrealm,
                                        pwObj, null);
    }

    // We should get here only, if we got an empty username pref from the reg.
    //ddump("  asking for username as well");

    var ret = false;
    try {
      var promptServ = GetPromptService();
      if (!promptServ)
        return false;

      ret = promptServ.promptUsernameAndPassword(window,
                                        dlgTitle, label, userObj, pwObj, 
                                        savePWLabel, savePWObj);

      if (!ret)
        this.transfer.cancel();
      else
        this.updateUsernamePasswordFromPrompt(userObj.value, pwObj.value,
                                              savePWObj.value);
    } catch (e) {
      dumpError(e);
    }
    //ddump("  done. result:");
    //ddump("  userObj.value: " + userObj.value);
    //ddump("  pwObj.value: " + pwObj.value);
    //ddump("  savePWObj.value: " + savePWObj.value);
    return ret;
  },
  // Update any data that the user supplied in a prompt dialog
  updateUsernamePasswordFromPrompt : function(username, password,savePassword)
  {
    // Set flag to save login data after transferring, if it changed in
    // dialog and the "SavePassword" checkbox was checked
    this.transfer.saveLogin = (username != this.transfer.username ||
                               password != this.transfer.password)
                              && savePassword;

    //ddump("  username: " + username);
    //ddump("  password: " + password);
    //ddump("  savePW: " + savePassword);
    //ddump("  saveLogin: " + this.transfer.saveLogin);

    this.transfer.username = username;
    this.transfer.password = password;
    this.transfer.savePassword = savePassword;

    var uri = GetIOService().newURI(this.transfer.remoteDir, null, null);
    uri.username = username;
    this.transfer.remoteDir = uri.spec; // doesn't work, the spec isn't updated
    this.transfer.dump();
  },
  makePWLabel : function (user, realm)
  {
  	if (!user)
      user = "";
    var label = GetString("EnterPasswordForUserAt");
    label = label.replace(/%username%/, user);
    label = label.replace(/%realm%/, realm);
    return label;
  },


  // XPCOM

  // nsISupports

  QueryInterface : function(aIID)
  {
    // ddump("QI: " + aIID.toString() + "\n");
    if (aIID.equals(Components.interfaces.nsIProgressEventSink)
     || aIID.equals(Components.interfaces.nsIRequestObserver)
     || aIID.equals(Components.interfaces.nsIStreamListener)
     || aIID.equals(Components.interfaces.nsISupports)
     || aIID.equals(Components.interfaces.nsISupportsWeakReference)
     || aIID.equals(Components.interfaces.nsIPrompt)
     || aIID.equals(Components.interfaces.nsIAuthPrompt)
     || aIID.equals(Components.interfaces.nsIFTPEventSink)
     || aIID.equals(Components.interfaces.nsIHttpEventSink)
     || aIID.equals(Components.interfaces.nsIDocShellTreeItem)
     || aIID.equals(Components.interfaces.nsIInterfaceRequestor))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  // nsIInterfaceRequestor

  getInterface : function(aIID)
  {
    return this.QueryInterface(aIID);
  }
}



var gIOService;
function GetIOService()
{
  if (gIOService)
    return gIOService;

  // throw errors into caller
  gIOService = Components.classes["@mozilla.org/network/io-service;1"]
               .getService(Components.interfaces.nsIIOService);
  return gIOService;
}
