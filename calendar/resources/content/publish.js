/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): ArentJan Banck <ajbanck@planet.nl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Original code:
// http://lxr.mozilla.org/mozilla/source/editor/ui/composer/content/publish.js
// TODO Implement uploadfile with:
//void setUploadFile(in nsIFile file, in string contentType, in long contentLength);

var gPublishHandler = null;


/* Create an instance of the given ContractID, with given interface */
function createInstance(contractId, intf) {
    return Components.classes[contractId].createInstance(Components.interfaces[intf]);
}


var gPublishIOService;
function GetIOService()
{
  if (gPublishIOService)
    return gPublishIOService;

  gPublishIOService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  if (!gPublishIOService)
    dump("failed to get io service\n");

  return gPublishIOService;
}


/**
 * This is the entry point into the file from calendar.js
 * contentType is always text/calendar
 */

function calendarPublish(aDataString, newLocation, login, password, contentType)
{
  try
  {
    var protocolChannel = get_destination_channel(newLocation, login, password);
    if (!protocolChannel)
    {
      dump("failed to get a destination channel\n");
      return;
    }
    output_string_to_channel(protocolChannel, aDataString, contentType);
    protocolChannel.asyncOpen(gPublishingListener, null);
  }
  catch (e)
  {
    alert("an error occurred in calendarPublish: " + e + "\n");
  }
}

function calendarUploadFile(aSourceFilename, newLocation, login, password, contentType)
{
   try
   {
      var protocolChannel = get_destination_channel(newLocation, login, password);
      if (!protocolChannel)
      {
         dump("failed to get a destination channel\n");
         return;
      }
      output_file_to_channel(protocolChannel, aSourceFilename, contentType);
      protocolChannel.asyncOpen(gPublishingListener, protocolChannel);

      return;
   }
   catch (e)
   {
      alert("an error occurred in calendarUploadFile: " + e + "\n");
   }
}


function output_string_to_channel( aChannel, aDataString, contentType )
{
   var uploadChannel = aChannel.QueryInterface(Components.interfaces.nsIUploadChannel);
   var postStream = createInstance('@mozilla.org/io/string-input-stream;1', 'nsIStringInputStream');

   // Create the stream
   postStream.setData(aDataString, aDataString.length);
   // Send the stream (data) through the channel
   uploadChannel.setUploadStream(postStream, contentType, -1);
}


function output_file_to_channel( aChannel, aFilePath, contentType )
{
   var uploadChannel = aChannel.QueryInterface(Components.interfaces.nsIUploadChannel);

   var thisFile = new File( aFilePath );

   thisFile.open( "r" );

   var theFileContents = thisFile.read();

   output_string_to_channel( aChannel, theFileContents, contentType );
}


// this function takes a login and password and adds them to the destination url
function get_destination_channel(destinationDirectoryLocation, login, password)
{
    var ioService = GetIOService();
    if (!ioService)
    {
       return null;
    }

    // create a channel for the destination location
    destChannel = create_channel_from_url(ioService, destinationDirectoryLocation, login, password);
    if (!destChannel)
    {
      dump("can't create dest channel\n");
      return null;
    }

    try
    {
	dump("about to set callbacks\n");
	destChannel.notificationCallbacks = window.docshell;  // docshell
	dump("notification callbacks set\n");
    }
    catch(e) {
	dump(e+"\n");
    }

    try {
       switch(destChannel.URI.scheme)
       {
         case 'http':
         case 'https':
	       return destChannel.QueryInterface(
		   Components.interfaces.nsIHttpChannel
		);
	   case 'ftp':
	       return destChannel.QueryInterface(
		   Components.interfaces.nsIFTPChannel
		);
	   case 'file':
	       return destChannel.QueryInterface(
		   Components.interfaces.nsIFileChannel
		);
	    default:
		return null;
	}
    }
    catch( e )
    {
       return null;
    }
}


// this function takes a full url, creates an nsIURI, and then creates a channel from that nsIURI
function create_channel_from_url(ioService, aURL, aLogin, aPassword)
{
  try
  {
    var nsiuri = ioService.newURI(aURL, null, null);
    if (!nsiuri)
      return null;
    if (aLogin)
    {
      nsiuri.username = aLogin;
      if (aPassword)
        nsiuri.password = aPassword;
    }
    return ioService.newChannelFromURI(nsiuri);
  }
  catch (e)
  {
    alert(e+"\n");
    return null;
  }
}

/*
function PublishCopyFile(srcDirectoryLocation, destinationDirectoryLocation, aLogin, aPassword)
{
  // append '/' if it's not there; inputs should be directories so should end in '/'
  if ( srcDirectoryLocation.charAt(srcDirectoryLocation.length-1) != '/' )
    srcDirectoryLocation = srcDirectoryLocation + '/';
  if ( destinationDirectoryLocation.charAt(destinationDirectoryLocation.length-1) != '/' )
    destinationDirectoryLocation = destinationDirectoryLocation + '/';

  try
  {
    // grab an io service
    var ioService = GetIOService();
    if (!ioService)
      return;

    // create a channel for the source location
    srcChannel = create_channel_from_url(ioService, srcDirectoryLocation, null, null);
    if (!srcChannel)
    {
      dump("can't create src channel\n");
      return;
    }

    // create a channel for the destination location
    var ftpChannel = get_destination_channel(destinationDirectoryLocation, aLogin, aPassword);
    if (!ftpChannel)
    {
      dump("failed to get ftp channel\n");
      return;
    }

    ftpChannel.open();
    protocolChannel.uploadStream = srcChannel.open();
    protocolChannel.asyncOpen(null, null);
    dump("done\n");
  }
  catch (e)
  {
    dump("an error occurred: " + e + "\n");
  }
}
*/

var gPublishingListener =
{
  QueryInterface: function(aIId, instance)
  {
    if (aIId.equals(Components.interfaces.nsIStreamListener) ||
        aIId.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  onStartRequest: function(request, ctxt)
  {
    dump("onStartRequest status = " + request.status.toString(16) + "\n");
  },

  onStopRequest: function(request, ctxt, status, errorMsg)
  {
    dump("onStopRequest status = " + request.status.toString(16) + " " + errorMsg + "\n");
  },

  onDataAvailable: function(request, ctxt, inStream, sourceOffset, count)
  {
    dump("onDataAvailable status = " + request.status.toString(16) + " " + count + "\n");
  }
}

