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

/* Make sure that we don't accidently overwrite files with newer
   or different changes. It will open a conflictResolve.xul dialog,
   if necessary. */

  /* First, we transfer the "listing" file from the server. This contains
     stats about the files on the server,
     like filenames, their last modification date and their filesizes.
     It is true that the same information could be obtained via the HTTP
     protocol (e.g. HTTP HEAD), but that would
     - be a lot harder to skip retrieval of already downloaded files,
       if we make only one connection per file,
       because we'd have to use modification time sent in the header right
       before the data and stop the transfer at the right time
     - be protocol-specific in the Mozilla implementation, unless we
       fix bug 186016 for all protocols we want to support
     - depend on the user's realtime clock to be correct
     The file format is XML, see example.

     A conflict is to have 2 versions of the same file, none a
     successor of the other (or the possibility thereof).

     Possible problems/conflicts:
     - Download
       - listing file transfer from server fails
       - listing from server is older than listing-uploaded
         (I don't think this can actually happen in practice, unless the
         clock is screwed)
       - listing-uploaded doesn't match profile files
         (incl. last download failed fatally)
       - download of individual files fails
     - Upload
       - listing file transfer from server fails
       - listing file from server is newer than listing-downloaded
         (other instance updated server while we were running or
         the last download failed)
       - listing file from server is newer than listing-uploaded
         Already covered by previous case / during download
       - listing from server is older than listing-downloaded
         (I don't think this can actually happen in practice, unless the
         clock is screwed)
       - upload of some files fails
       - upload fails fatally (crash etc.)
       - Mozilla crashed/term. before it had a chance to even start the upload

     Checks/Steps:
     - Download
       1. Transfer listing file from server to listing.xml
       2. Compare listing uploaded with listing.xml from server.
          - if they match, then we already have the newest version locally and
            don't need to download and don't have a problem/conflict either,
            even if the local files don't match listing.xml from server.
            We can just copy the listing.xml from server to
            listing-downloaded and be done.
          - if they don't match, then either
            - another instance uploaded after
              we ran the last time, which is not a problem in itself, but we
              need to download and make more checks for conflicts.
            - our last upload finished only partially, which we will consider
              a problem and a conflict XXX
            2a. Compare local files with listing-uploaded
                - if they match, the server has/had our newest local version, so
                  there are no conflicts, so download
                - if they don't match, this profile ran (without uploading)
                  after the load upload, so
                  2aa. Compare remote listing with local files
                       - If they match, listing-uploaded was wrong (maybe the
                         current files were transmitted by something else), so
                         there is no problem; ignore it and go on
                       - If they don't match, use the result for conflict
                         resolution
       3. If there were conflicts, ask the user what to do
       4. Download files
       5. If transfer succeeded, move listing.xml to listing-downloaded.
          Note that we do that independent of the choices in conflict
          resolution. If the user made his choice, we assume that we doesn't
          care about the other files, so here we just pretend that we used
          them, so that conflict resolution in later runs doesn't come up
          for the same conflict anymore.
       5. Unused: (not sure, if needed or even a good idea)
          Check local files against listing.xml.
          - if they match (esp. filesizes), all went fine. Move listing.xml
            to listing-downloaded.
          - if they don't match, we have a severe problem. We just overwrote
            the local files (which are assumed to be good) with broken
            versions. Warn the user. Remove listing-downloaded, which
            will cause conflicts during the next upload, which allows the
            user to then prevent overwriting of the file on the server, which
            might still be OK. This won't help, if the upload failed although
            netwerk reported success, but I don't know how to protect against
            that apart from a temporary second copy of the files.
     - Upload
       1. Transfer listing file from server to listing.xml
       2. Compare listing-downloaded with listing.xml from server
          - if they match, our local version is the newest, so there is no
            conflict.
          - if they don't match, then either
            - another instance uploaded while we ran. classic conflict.
              listing.xml won't match listing-uploaded in this case, so we
              can use the checks for the next case.
            - the last download failed.
            2a. Compare listing.xml from the server with listing-uploaded
                - If they match, then we don't have a problem here,
                  because we were the last ones who uploaded
                  and we probably have the same info still in the local files.
                - if the don't match, then another instance uploaded after
                  we last uploaded, and we didn't get that info yet, so we have
                  a conflict.
       3. If there were conflicts, ask the user what to do        
       4. Upload profile files
       5  Create listing file for server, based on conflict choices and
          upload success, containing
          - the entries from the server's listing file for
            those files which we chose not to upload /plus/
          - the entries for those local files which successfully uploaded
       6. Upload listing file (created in last step)
       7. Create listing-uploaded, not containing those files for which
          the user selected the server version (otherwise, we would think
          in the next download run that we have them already in the local
          profile, not download them and then overwrite the server file
          during the next upload)

     I know, it's complicated. If somebody knows a better solution
     (less steps and more cases covered, ideally provably all cases covered),
     ...
  */


const kListingTransferFilename = "listing.xml";
      /* the server listing file we just downloaded or are uploading atm */
const kListingDownloadedFilename = "listing-downloaded.xml";
              /* last version of listing file we downloaded from server,
                 when the files download was successful.
                 Unlike listing-uploaded, it does *not* necessarily list
                 the files we downloaded (e.g. in case of conflicts). */
const kListingUploadedFilename = "listing-uploaded.xml";
              /* last version of files we uploaded from this instance.
                 In case of conflicts etc., this may be different from the
                 listing file we uploaded to the server (which includes the
                 files not uploaded, but kept on the server). */
const kListingMimetype = "text/xml";
const kProfileFilesMimetype = "application/octet-stream";
const kConflDlg = "chrome://sroaming/content/transfer/conflictResolve.xul";

var gLocalDir;

/* A few common data types used here:
   FilesStats  Array of FileStats  Like transfer.files
   FileStats  Object with properties
                   filename  String  path to the file, incl. possible dirs,
                                     relative to the profile dir.
                   mimetype  String  e.g. "application/octet-stream"
                   lastModified  int, Unix time
                   size  int, in bytes
   FilesList  Array of Objects with property
                   filename  String  See above
                   (i.e. a FilesStats with possibly fewer properties)
*/


/*
  Main (and only) entry function for the routines in this file.

  Starts and performs the replication of needed files from the server
  to the local profile dir or vice-versa. Checks for conflicts and asks the
  user, if there is a conflict.

  @param transfer  Transfer object  For the main profile files. We'll create a
                        second Transfer object, based on this one, for the
                        listing file.
  @param listingProgress  function(bool)  Callback to tell the caller, if
                        the transfer of the listing file is in progress.
                        The caller will probably display an indetermined
                        progressmeter, if the parameter is true.
  @result  nothing
 */
function checkAndTransfer(transfer, listingProgress)
{
  //ddump("checking deps");

  gLocalDir = transfer.localDir;

  /* A few nested callbacks follow. Asyncronous processing fun... :-/
     Some need local vars, I don't want to use global vars for that. */

  //ddump("Step 1: Downloading listing file from server");
  remove(kListingTransferFilename);
  var filesListing = new Array();
  filesListing[0] = new Object();
  filesListing[0].filename = kListingTransferFilename;
  filesListing[0].mimetype = kListingMimetype;
  filesListing[0].size = undefined;
  SetListingTransfer(true);
  var listingTransfer = new Transfer(true, // download
                                     transfer.serial,
                                     transfer.localDir, transfer.remoteDir,
                                     transfer.password, transfer.savePassword,
                                     filesListing,
                                     function(success)
  {
    try {
      SetListingTransfer(false);
      addFileStatus(listingTransfer.files[0]);

      /* In case we had to ask for the password,
         save it for the main transfer as well */
      transfer.username = listingTransfer.username;
      transfer.password = listingTransfer.password;
      transfer.savePassword = listingTransfer.savePassword;
      transfer.saveLogin = listingTransfer.saveLogin;
      //ddump("loading remote listing file");

      var remoteListingResult = loadAndReadListingFile(success
                                                       ? kListingTransferFilename
                                                       : null,
                                                       function()
      {
        try {
          var remoteListing = remoteListingResult.value;
          //ddumpObject(remoteListing, "remoteListing");
          if (transfer.download)
            download(transfer, remoteListing);
          else
            upload(transfer, remoteListing);
        } catch (e) { lastCatch(e, transfer); }
      });
    } catch (e) { lastCatch(e, transfer); }
  }, function()
  {
  });
  listingTransfer.transfer();
}

// download part of checkAndTransfer
function download(transfer, remoteListing)
{
  //ddump("loading listing-uploaded file");
  var listingUploadedResult = loadAndReadListingFile(kListingUploadedFilename,
                                                     function()
  {
    try {
      var listingUploaded = listingUploadedResult.value;
      var localFiles;

      //ddump("Step 2: Comparing listing from server with listing-uploaded");
      var comparisonStep2 = compareFiles(transfer.files,
                                         remoteListing,
                                         listingUploaded);
      var onServerChangedFiles = comparisonStep2.mismatches;
           // files that have been changed by another instance since we uploaded
      if (onServerChangedFiles.length == 0)
        // Match: files didn't change on the server since our last upload
      {
        //ddump("Server files didn't change since last upload from here, so");
        //ddump("we're done, no need to actually download.");
        move(kListingTransferFilename, kListingDownloadedFilename);
        onCancel();
        return;
      }
      else // Mismatch
      {
        localFiles = localFilesStats(transfer.files);
        // non-existant files
        var missingRemote = substractFiles(transfer.files, remoteListing);
        var missingLocal = substractFiles(transfer.files, localFiles);

        //ddump("Step 2a: Comparing local files with listing-uploaded");
        var comparisonStep2a = compareFiles(onServerChangedFiles,
                                            localFiles,
                                            listingUploaded);
        var onServerAndLocallyChangedFiles = comparisonStep2a.mismatches;
             // files that have been changed in this instance since we uploaded
             // and that have *also* been changed on the server by another
             // instance since we uploaded
        if (onServerAndLocallyChangedFiles.length == 0)
        {
          //ddump("Step 3: Conflict dialog skipped, because no conflict found");
          // no conflict, but we need to download

          transfer.files = extractFiles(transfer.files, onServerChangedFiles);
               // download only files which actually changed
          transfer.files = substractFiles(transfer.files, missingRemote);
               // don't bail, if e.g. somebody deleted them on the server
          transfer.filesChanged();
        }
        else
        {
          //ddump("Step 2aa: Comparing remote listing with local files");
          var comparisonStep2aa = compareFiles(onServerAndLocallyChangedFiles,
                                               remoteListing,
                                               localFiles);

          // avoid conflicts for files which don't exist on server or locally
          var conflicts = substractFiles(comparisonStep2aa.mismatches,
                                         addFiles(missingRemote, missingLocal));
	      var keepLocalVersionFiles = missingRemote;

          if (conflicts.length > 0)
          {
            //ddump("Step 3: Asking user which version to use");
            var answer = conflictAsk(true,
                                     conflicts,
                                     remoteListing,
                                     localFiles);
                 /* for the mismatching files, pass the stats of the server
                    and local versions to the dialog */
            if (answer.button != 3) // cancel pressed
            {
              onCancel();
              return;
            }
            keepLocalVersionFiles = addFiles(keepLocalVersionFiles, answer.local);
          }
          if (keepLocalVersionFiles.length > 0)
          {
            transfer.files = substractFiles(transfer.files, keepLocalVersionFiles);
            transfer.filesChanged();
          }
        }
      }

      //ddump("Step 4: Downloading profile files");
      transfer.finishedCallbacks.push(function(success)
      {
        try {
          /*
          //ddumpCont("Step 5: Check local files (incl. downloaded ones)");
          //ddump(" against server listing");
          var localFiles = localFilesStats(remoteListing);
          checkFailedFiles(transfer);
          */

          //ddump("Step 5");
          if (success)
            /* Profile files should now match the listing.xml from the server.
               Move listing.xml from server to listing-downloaded, to prevent
               overwriting during the next update and to record, which is the
               last version we got from the server, for future checks. */
            move(kListingTransferFilename, kListingDownloadedFilename);

          //ddump("transfer done");

          CheckDone(true); // progress dialog - close it, if possible
        } catch (e) { lastCatch(e, transfer); }
      });
      transfer.transfer();
    } catch (e) { lastCatch(e, transfer); }
  }, false);
  // if you add anything here, pay attention for Cancel (see above)
}

// upload part of checkAndTransfer
function upload(transfer, remoteListing)
{
  //ddump("loading listing-downloaded file");
  var listingDownloadedResult = loadAndReadListingFile(
                                               kListingDownloadedFilename,
                                               function()
  {
    try {
      var listingDownloaded = listingDownloadedResult.value;
      var localFiles = localFilesStats(transfer.files);
      // non-existant files
      var missingRemote = substractFiles(transfer.files, remoteListing);
      var missingLocal = substractFiles(transfer.files, localFiles);

      //ddump("Step 2: Comparing listing from server with listing-downloaded");
      var comparisonStep2 = compareFiles(transfer.files,
                                         remoteListing,
                                         listingDownloaded);
      var onServerChangedFiles = comparisonStep2.mismatches;
      if (onServerChangedFiles.length == 0)
        // Match: files didn't change on the server since our last download
      {
        //ddump("Step 3: Conflict dialog skipped, because no conflict found");
        uploadStep4(transfer, localFiles, remoteListing, missingLocal);
      }
      else // Mismatch
      {
        //ddump("Step 2a: Comparing listing from server with listing-uploaded");
        //ddump("loading listing-uploaded file");
        var listingUploadedResult = loadAndReadListingFile(
                                                     kListingUploadedFilename,
                                                     function()
        {
          try {
            var listingUploaded = listingUploadedResult.value;

            var comparisonStep2a = compareFiles(onServerChangedFiles,
                                                remoteListing,
                                                listingUploaded);

            // avoid conflicts for files which don't exist on server or locally
            var conflicts = substractFiles(comparisonStep2a.mismatches,
                                           addFiles(missingRemote, missingLocal));
            var keepServerVersionFiles = missingLocal;

            if (conflicts.length > 0)
            {
              //ddump("Step 3: Asking user which version to use");
              var answer = conflictAsk(false,
                                       conflicts,
                                       remoteListing,
                                       localFiles);
                     /* for the mismatching files, pass the stats of the server
                        and local versions to the dialog */
              if (answer.button != 3) // cancel pressed
              {
                onCancel();
                return;
              }
              keepServerVersionFiles = addFiles(keepServerVersionFiles,
                                                answer.server);
            }
            uploadStep4(transfer, localFiles, remoteListing,
                        keepServerVersionFiles);
          } catch (e) { lastCatch(e, transfer); }
        }, false);
      }
    } catch (e) { lastCatch(e, transfer); }
  }, false);
}

function uploadStep4(transfer, localFiles, remoteFiles, keepServerVersionFiles)
{
  if (keepServerVersionFiles)
  {
    transfer.files = substractFiles(transfer.files, keepServerVersionFiles);
    transfer.filesChanged();
  }

  //ddump("Step 4: Uploading profile files");
  transfer.finishedCallbacks.push(function(success)
  {
    try {
      //ddump("Step 5: Generating listing file for server based on facts");
      var filesDone = extractFiles(localFiles,
                                   transfer.filesWithStatus("done"));
      createListingFile(addFiles(filesDone,
                                 substractFiles(remoteFiles, filesDone)),
                             // for overlaps, take stats from filesDone
                        kListingTransferFilename);

      //ddump("Step 6: Uploading listing file to server");
      var filesListing = new Array();
      filesListing[0] = new Object();
      filesListing[0].filename = kListingTransferFilename;
      filesListing[0].mimetype = kListingMimetype;
      filesListing[0].size = undefined;
      SetListingTransfer(true);
      var listingTransfer = new Transfer(false, // upload
                                         transfer.serial,
                                         transfer.localDir, transfer.remoteDir,
                                         transfer.password,
                                         transfer.savePassword,
                                         filesListing,
                                         function(success)
      {
        try {
          addFileStatus(listingTransfer.files[0]);
          SetListingTransfer(false);

          //ddump("Step 7: Generate listing-uploaded");
          remove(kListingTransferFilename);
          createListingFile(substractFiles(localFiles, keepServerVersionFiles),
                            kListingUploadedFilename);

          //ddump("transfer done");

          CheckDone(true);
        } catch (e) { lastCatch(e, transfer); }
      }, function()
      {
      });
      listingTransfer.transfer();
    } catch (e) { lastCatch(e, transfer); }
  });
  transfer.transfer();
}

/* catch any unexpected errors here instead of throwing them into caller. */
function lastCatch(e, transfer)
{
  //GlobalError(e);
  ///* alternative implementation:
  //ddump("Unexpected exception: " + e);
  //ddump("abort");
  SetStatusMessage(e.toString());
  for (var i = 0; i < transfer.files.length; i++)
  {
    transfer.files[i].status = "failed";
    transfer.files[i].statusCode = kErrorAbort;
    SetProgressStatus(i);
  }
  CheckDone(true);
  //*/
}


/*
  We have a conflict, i.e. 2 versions of the same file, none a direct
  successor of the other. Ask the user, which version to use (the other one
  will be overwritten) - server or local. Asks for several files at once,
  but the user can make decisions for each file individually.

  @param download  boolean
  @param filesList    FilesList   The files which have conflicts.
  @param serverFiles  FilesStats  The versions of the
                                  files as they are on the server.
                                  Files must be a superset of filesList.
  @param localFiles   FilesStats  The versions of the files as they are
                                  in the local profile.
                                  Files must be a superset of filesList.
  @result  Object with properties |button|, |server| and |local|.
                             |button|: 3 == OK, 4 == Cancel
                             |server| and |local| are both |FilesList|s,
                             if OK clicked.
                             Each file will be *either* in |local| or |server|.
                             If a fatal error occurred or the user pressed
                             Cancel, both arrays will be empty.
 */
function conflictAsk(download, filesList, serverFiles, localFiles)
{
  var result = new Object();
  result.server = new Array();
  result.local = new Array();
  try
  {
    // pass data to dialog
    // doc see mozSRoaming.cpp
    var param = Components
                .classes["@mozilla.org/embedcomp/dialogparam;1"]
                .createInstance(Components.interfaces.nsIDialogParamBlock);
    param.SetInt(0, download ? 1 : 2);
    param.SetInt(1, filesList.length);
    for (var i = 0, l = filesList.length; i < l; i++)
    {
      var filename = filesList[i].filename;
      var sf = findEntry(filename, serverFiles);
      var lf = findEntry(filename, localFiles);
      param.SetString(i + 1, filename
                         + (sf ? "," + sf.lastModified + "," + sf.size : ",,")
                         + (lf ? "," + lf.lastModified + "," + lf.size : ",,")
                      );
    }

    // invoke dialog, wait for return
    var windowWatcher = Components
                        .classes["@mozilla.org/embedcomp/window-watcher;1"]
                        .getService(Components.interfaces.nsIWindowWatcher);
    windowWatcher.openWindow(window, kConflDlg, null,
                             "centerscreen,chrome,modal,titlebar",
                             param);
    result.button = param.GetInt(0);
    if (result.button != 3) // Not OK (Cancel or invalid)
      return result;

    // read result from dialog
    /* I am assuming that the sequence of iteration here is the same as in the
       last |for| statement. If that is not true, the indices gotten from
       param block will not match the array and we will interpret the result
       wrongly. */
    for (var i = 0, l = filesList.length; i < l; i++)
    {
      var value = param.GetInt(i + 1);
      if (value == 1) // use server version
        result.server.push(filesList[i]);
      else if (value == 2) // use local version
        result.local.push(filesList[i]); // *cough*
    }
  }
  catch (e)
  {
    //ddump(e);
  }
  return result;
}


/* Create nsIFile. See filesList.js.
   @param file  String  filename/path relative to the local (profile) dir
   @result nsIFile
*/
function makeNSIFileFromRelFilename(filename)
{
  var localDir = GetIOService().newURI(gLocalDir, null, null)
                   .QueryInterface(Components.interfaces.nsIFileURL)
                   .file.clone();
  localDir.append(filename);
  return localDir;
}
