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

/* Provides some global functions useful for implementing the backend or UI
   of the file transfer.
*/


////////////////////////////////////////////////////////////////////////////
// UI
////////////////////////////////////////////////////////////////////////////

var gStringBundle;
// Get string from standard string bundle (transfer.properties)
function GetString(name)
{
  if (!gStringBundle)
    gStringBundle = document.getElementById("bundle_roaming_transfer");
  try {
    return gStringBundle.getString(name);
  } catch (e) {
    return null;
  }
}

function GetStringWithFile(stringname, filename)
{
  return GetString(stringname).replace(/%file%/, filename);
}

// See also GetFileDescription() below

function GetPromptService()
{
  // no caching, not worth it
  // throw errors into caller
  return Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                   .getService(Components.interfaces.nsIPromptService);
}


////////////////////////////////////////////////////////////////////////////
// Error handling (UI)
////////////////////////////////////////////////////////////////////////////
// These functions return a string for display to the user.

/* Accepts either
   - XPCOM exceptions (with nsresult codes in e.result)
   - our own errors (e.prop is the name of the error in our string bundle)
   - objects convertable to string, will be shown to user verbatim.
     (Only for unexpected errors, because not localized.)
*/
function ErrorMessageForException(e)
{
  if (!e)
    return "null";
  else if (e.result)
    return ErrorMessageForStatusCode(e.result);
  else if (e.prop)
    return GetString(e.prop);
  else
    return e.toString();
}

/* Translates the result of a file transfer into a String for display
   to the user.

   @param aFile  Object from transfer.files[]
   @return String
*/
function ErrorMessageForFile(aFile)
{
  if (aFile.statusCode == kStatusHTTP)
    return GetStringWithFile(aFile.status == "done"
                             ? "HTTPResponseSucceeded"
                             : "HTTPResponseFailed",
                             aFile.filename)
           .replace(/%responsecode%/, aFile.httpResponse)
           .replace(/%responsetext%/, aFile.statusText);
  else if (aFile.statusCode == kErrorFailure && aFile.statusText) // FTP
    return GetStringWithFile("StatusTextOnly", aFile.filename)
           .replace(/%statustext%/, aFile.statusText);
  else
    return ErrorMessageForStatusCode(aFile.statusCode, aFile.filename);
}

/* Translates an XPCOM result code into a String for display to the user.

   @param aStatusCode  int  XPCOM result code
   @param aFilename  String  relative filename
   @return String
*/
function ErrorMessageForStatusCode(aStatusCode, aFilename)
{
  var statusMessage = null;
  switch (aStatusCode)
  {
    case 0:
      statusMessage = GetStringWithFile("NSOK", aFilename);
      break;
    case kFileNotFound:
      gFileNotFound = true;
      statusMessage = GetStringWithFile("FileNotFound", aFilename);
      break;
    case kNetReset:
      // We get this when subdir doesn't exist AND
      //   if aFilename used is same as an existing subdir 
      var dir = (gTransferData.filename == aFilename) ? gTransferData.docDir
                                                      : gTransferData.otherDir;

      if (dir)
      {
        // This is the ambiguous case when we can't tell,
        // if subdir or filename is bad
        statusMessage = GetString("SubdirDoesNotExist").replace(/%dir%/, dir);
        statusMessage = statusMessage.replace(/%file%/, aFilename);

        // Remove directory from saved prefs
        // XXX Note that if subdir is good,
        //     but filename = next level subdirectory name,
        //     we really shouldn't remove subdirectory,
        //     but it's impossible to differentiate this case!
        RemoveTransferSubdirectoryFromPrefs(gTransferData, dir);
      }
      else
        statusMessage = GetStringWithFile("FilenameIsSubdir", aFilename);
      break;
    case kErrorMalformedURI:
      statusMessage = GetString("MalformedURI");
      break;
    case kNotConnected:
    case kConnectionRefused:
    case kNetTimeout:
    case kUnknownHost:
    case kPortAccessNotAllowed:
    case kUnknownProxyHost:
      statusMessage = GetString("ServerNotAvailable");
      break;
    case kOffline:
      statusMessage = GetString("Offline");
      break;
    case kDiskFull:
    case kNoDeviceSpace:
      statusMessage = GetStringWithFile("DiskFull", aFilename);
      break;
    case kNameTooLong:
      statusMessage = GetStringWithFile("NameTooLong", aFilename);
      break;
    case kAccessDenied:
      statusMessage = GetStringWithFile("AccessDenied", aFilename);
      break;
    case kStatusFTPCWD:
      statusMessage = GetStringWithFile("FTPCWD", aFilename);
      break;
    case kErrorAbort:
      statusMessage = GetStringWithFile("Abort");
      break;
    case kUnknownType:
    default:
      statusMessage = GetStringWithFile("UnknownTransferError", aFilename)
                                        + " " + NameForStatusCode(aStatusCode);
      break;
  }
  return statusMessage
}



////////////////////////////////////////////////////////////////////////////
// Error handling (general)
////////////////////////////////////////////////////////////////////////////
// Internal, backend, debugging stuff

/*
  This is bad. We should have this in a central place, but I don't know
  (nor could find) one where this is available. Note that I do use
  |Components.results| in the end, but it doesn't have all the errors I need.
 */

// Transfer error codes
//   These are translated from C++ error code strings like this:
//   kFileNotFound = "NS_FILE_NOT_FOUND",
const kNS_OK = 0;
const kMyBase = 0x80780000; // Generic module, only valid within our module
const kNetBase = 2152398848; // 0x804B0000
const kFilesBase = 2152857600; // 0x80520000
const kUnknownType = kFilesBase + 04; // nsError.h
const kDiskFull = kFilesBase + 10; // nsError.h
const kNoDeviceSpace = kFilesBase + 16; // nsError.h
const kNameTooLong = kFilesBase + 17; // nsError.h
const kFileNotFound = kFilesBase + 18; // nsError.h, 0x80520012
const kAccessDenied = kFilesBase + 21; // nsError.h
const kNetReset = kNetBase + 20;
const kErrorMalformedURI = kNetBase + 10; // netCore.h
const kNotConnected = kNetBase + 12; // netCore.h
const kConnectionRefused = kNetBase + 13;
const kNetTimeout = kNetBase + 14;
const kInProgress = kNetBase + 15; // netCore.h
const kOffline = kNetBase + 16; // netCore.h; 0x804b0010
const kUnknownHost = kNetBase + 30; // nsNetErr; was "no connection or timeout"
const kUnknownProxyHost = kNetBase + 42; // nsNetError.h
const kPortAccessNotAllowed = kNetBase + 19; // netCore.h
const kErrorDocumentNotCached = kNetBase + 70; // nsNetError.h
const kErrorBindingFailed = kNetBase + 1; // netCore.h
const kErrorBindingAborted = kNetBase + 2; // netCore.h
const kErrorBindingRedirected = kNetBase + 3; // netCore.h
const kErrorBindingRetargeted = kNetBase + 4; // netCore.h
const kStatusBeginFTPTransaction = kNetBase + 27; // ftpCore.h
const kStatusEndFTPTransaction = kNetBase + 28; // ftpCore.h
const kStatusFTPLogin = kNetBase + 21; // ftpCore.h
const kStatusFTPCWD = kNetBase + 22; // ftpCore.h
const kStatusFTPPassive = kNetBase + 23; // ftpCore.h
const kStatusFTPPWD = kNetBase + 24; // ftpCore.h
const kErrorNoInterface = 0x80004002; // nsError.h
const kErrorNotImplemented = 0x80004001; // nsError.h
const kErrorAbort = 0x80004004; // nsError.h
const kErrorFailure = 0x80004005; // nsError.h
const kErrorUnexpected = 0x8000ffff; // nsError.h
const kErrorInvalidValue = 0x80070057; // nsError.h
const kErrorNotAvailable = 0x80040111; // nsError.h
const kErrorNotInited = 0xC1F30001; // nsError.h
const kErrorAlreadyInited = 0xC1F30002; // nsError.h
const kErrorFTPAuthNeeded = 0x4B001B; // XXX not sure what exactly this is or
   // where it comes from (grep doesn't find it in dec or hex notation), but
   // that's what I get when the credentials are not accepted by the FTP server
const kErrorFTPAuthFailed = 0x4B001C; // dito
const kStatusHTTP = kMyBase + 0;
/* See Transfer.onStatus().
   *_Status are the number we get from nsIProgressEventSink, the others
   are what we use internally in the rest of the code. */
const kStatusResolvingHost = kMyBase + 5;
const kStatusConnectedTo = kMyBase + 6;
const kStatusSendingTo = kMyBase + 7;
const kStatusReceivingFrom = kMyBase + 8;
const kStatusConnectingTo = kMyBase + 9;
const kStatusWaitingFor = kMyBase + 10;
const kStatusReadFrom = kMyBase + 11;
const kStatusWroteTo = kMyBase + 12;
const kStatusResolvingHost_Status = kNetBase + 3;// nsISocketTransport.idl
const kStatusConnectedTo_Status = kNetBase + 4; // nsISocketTransport.idl
const kStatusSendingTo_Status = kNetBase + 5; // nsISocketTransport.idl
const kStatusReceivingFrom_Status = kNetBase + 6; // nsISocketTransport.idl
const kStatusConnectingTo_Status = kNetBase + 7; // nsISocketTransport.idl
const kStatusWaitingFor_Status = kNetBase + 10; // nsISocketTransport.idl
const kStatusReadFrom_Status = kNetBase + 8; // nsIFileTransportService.idl
const kStatusWroteTo_Status = kNetBase + 9; // nsIFileTransportService.idl

// Translates an XPCOM result code into a String similar to the C++ constant.
function NameForStatusCode(aStatusCode)
{
  switch (aStatusCode)
  {
    case 0:
      return "NS_OK";
    case kStatusReadFrom:
      return "NET_STATUS_READ_FROM";
    case kStatusWroteTo:
      return "NET_STATUS_WROTE_TO";
    case kStatusReceivingFrom:
      return "NET_STATUS_RECEIVING_FROM";
    case kStatusSendingTo:
      return "NET_STATUS_SENDING_TO";
    case kStatusWaitingFor:
      return "NET_STATUS_WAITING_FOR";
    case kStatusResolvingHost:
      return "NET_STATUS_RESOLVING_HOST";
    case kStatusConnectedTo:
      return "NET_STATUS_CONNECTED_TO";
    case kStatusConnectingTo:
      return "NET_STATUS_CONNECTING_TO";
    case kStatusHTTP:
      return "See HTTP response";
    case kErrorBindingFailed:
      return "BINDING_FAILED";
    case kErrorBindingAborted:
      return "BINDING_ABORTED";
    case kErrorBindingRedirected:
      return "BINDING_REDIRECTED";
    case kErrorBindingRetargeted:
      return "BINDING_RETARGETED";
    case kErrorMalformedURI:
      return "MALFORMED_URI";
    case kNetBase + 11: // netCore.h
      return "ALREADY_CONNECTED";
    case kNotConnected:
      return "NOT_CONNECTED";
    case kConnectionRefused:
      return "CONNECTION_REFUSED";
    case kNetTimeout:
      return "NET_TIMEOUT";
    case kInProgress:
      return "IN_PROGRESS";
    case kOffline:
      return "OFFLINE";
    case kNetBase + 17: // netCore.h
      return "NO_CONTENT";
    case kNetBase + 18: // netCore.h
      return "UNKNOWN_PROTOCOL";
    case kPortAccessNotAllowed:
      return "PORT_ACCESS_NOT_ALLOWED";
    case kNetReset:
      return "NET_RESET";
    case kStatusFTPLogin:
      return "FTP_LOGIN";
    case kStatusFTPCWD:
      return "FTP_CWD";
    case kStatusFTPPassive:
      return "FTP_PASV";
    case kStatusFTPPWD:
      return "FTP_PWD";
    case kUnknownHost:
      return "UNKNOWN_HOST or NO_CONNECTION_OR_TIMEOUT";
    case kUnknownProxyHost:
      return "UNKNOWN_PROXY_HOST";
    case kErrorFTPAuthNeeded:
      return "FTP auth needed (?)";
    case kErrorFTPAuthFailed:
      return "FTP auth failed (?)";
    case kStatusBeginFTPTransaction:
      return "NET_STATUS_BEGIN_FTP_TRANSACTION";
    case kStatusEndFTPTransaction:
      return "NET_STATUS_END_FTP_TRANSACTION";
    case kNetBase + 61:
      return "NET_CACHE_KEY_NOT_FOUND";
    case kNetBase + 62:
      return "NET_CACHE_DATA_IS_STREAM";
    case kNetBase + 63:
      return "NET_CACHE_DATA_IS_NOT_STREAM";
    case kNetBase + 64:
      return "NET_CACHE_WAIT_FOR_VALIDATION"; // XXX error or status?
    case kNetBase + 65:
      return "NET_CACHE_ENTRY_DOOMED";
    case kNetBase + 66:
      return "NET_CACHE_READ_ACCESS_DENIED";
    case kNetBase + 67:
      return "NET_CACHE_WRITE_ACCESS_DENIED";
    case kNetBase + 68:
      return "NET_CACHE_IN_USE";
    case kErrorDocumentNotCached:
      return "NET_DOCUMENT_NOT_CACHED";//XXX error or status? seems to be error
    case kFilesBase + 1: // nsError.h
      return "UNRECOGNIZED_PATH";
    case kFilesBase + 2: // nsError.h
      return "UNRESOLABLE SYMLINK";
    case kFilesBase + 4: // nsError.h
      return "UNKNOWN_TYPE";
    case kFilesBase + 5: // nsError.h
      return "DESTINATION_NOT_DIR";
    case kFilesBase + 6: // nsError.h
      return "TARGET_DOES_NOT_EXIST";
    case kFilesBase + 8: // nsError.h
      return "ALREADY_EXISTS";
    case kFilesBase + 9: // nsError.h
      return "INVALID_PATH";
    case kDiskFull:
      return "DISK_FULL";
    case kFilesBase + 11: // nsError.h
      return "FILE_CORRUPTED (justice department, too)";
    case kFilesBase + 12: // nsError.h
      return "NOT_DIRECTORY";
    case kFilesBase + 13: // nsError.h
      return "IS_DIRECTORY";
    case kFilesBase + 14: // nsError.h
      return "IS_LOCKED";
    case kFilesBase + 15: // nsError.h
      return "TOO_BIG";
    case kNoDeviceSpace:
      return "NO_DEVICE_SPACE";
    case kNameTooLong:
      return "NAME_TOO_LONG";
    case kFileNotFound:
      return "FILE_NOT_FOUND";
    case kFilesBase + 19: // nsError.h
      return "READ_ONLY";
    case kFilesBase + 20: // nsError.h
      return "DIR_NOT_EMPTY";
    case kAccessDenied:
      return "ACCESS_DENIED";
    default:
      for (a in Components.results)
        if (Components.results[a] == aStatusCode)
          return a;
      return String(aStatusCode);
  }
}



////////////////////////////////////////////////////////////////////////////
// Debugging (backend and UI)
////////////////////////////////////////////////////////////////////////////

// Turn this on to get debug output.
const debugOutput = true;
function ddump(text)
{
  if (debugOutput)
    dump(text + "\n");
}
function ddumpCont(text)
{
  if (debugOutput)
    dump(text);
}
function dumpObject(obj, name, maxDepth, curDepth)
{
  if (!debugOutput)
    return;
  if (curDepth == undefined)
    curDepth = 0;
  if (maxDepth != undefined && curDepth > maxDepth)
    return;

  var i = 0;
  for (prop in obj)
  {
    i++;
    if (typeof(obj[prop]) == "object")
    {
      if (obj[prop] && obj[prop].length != undefined)
        ddump(name + "." + prop + "=[probably array, length "
              + obj[prop].length + "]");
      else
        ddump(name + "." + prop + "=[" + typeof(obj[prop]) + "]");
      dumpObject(obj[prop], name + "." + prop, maxDepth, curDepth+1);
    }
    else if (typeof(obj[prop]) == "function")
      ddump(name + "." + prop + "=[function]");
    else
      ddump(name + "." + prop + "=" + obj[prop]);
  }
  if (!i)
    ddump(name + " is empty");    
}
function dumpError(text)
{
  dump(text + "\n");
}



///////////////////////////////////////////////////////////////////////
// Profile-related stuff
// Only makes sense for roaming
///////////////////////////////////////////////////////////////////////

/* Returns a human-readable name for a profile file, e.g.
   "abook.mab" -> "Personal Addressbook"
   Will look for a stringbundle match
   - for filename
   - if failing that, for id
   - if failing that, use the raw id
   - if failing that, use the raw filename
   @param filename  string
   @param id  in case the filename is changing for a certain profile file
              (e.g. in the case of password files), optionally pass an ID
              for the file here, as used in filedescr.properties.
              May be null.
   @return string  Description for file
*/
var gStringBundleFiledescr;
function GetFileDescription(filename, id)
{
  if (!gStringBundleFiledescr)
    gStringBundleFiledescr =
                          document.getElementById("bundle_roaming_filedescr");
  try {
    return gStringBundleFiledescr.getString(filename);
  } catch (e) {
    try {
      return gStringBundleFiledescr.getString(id);
    } catch (e2) {
      if (id)
        return id;
      return filename;
    }
  }
}
