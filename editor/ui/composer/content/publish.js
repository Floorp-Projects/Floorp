function EditorPublish(destinationDirectoryLocation, fileName, login, password)
{
  post_current_editor_contents_to_server(destinationDirectoryLocation, fileName, login, password);
//  PublishCopyFile(srcDirectoryLocation, destinationDirectoryLocation, fileName);
}

// this function takes a login and password and adds them to the destination url
function get_destination_channel(destinationDirectoryLocation, fileName, login, password)
{
  try
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    if (!ioService)
    {
      dump("failed to get io service\n");
      return;
    }

    // create a channel for the destination location
    destChannel = create_channel_from_url(ioService, destinationDirectoryLocation + fileName, login, password);
    if (!destChannel)
    {
      dump("can't create dest channel\n");
      return;
    }
    // destChannel.SetNotificationCallbacks();

    var ftpChannel = destChannel.QueryInterface(Components.interfaces.nsIFTPChannel);
    return ftpChannel;
  }
  catch (e)
  {
    return null;
  }
}

function output_current_editor_to_stream()
{
  var formatType = editorShell.editorType;
  dump("formatType is " + formatType + "\n");
  var flags = 256; // nsIDocumentEncoder::OutputEncodeEntities
  var charset = editorShell.GetDocumentCharacterSet();
  dump("charset is " + charset + "\n");
  // XXX we need to create an nsIOutputStream here
  var stream = editorShell.editor.OutputToStream('text/'+formatType, charset, flags); 
  return stream;
}

function post_current_editor_contents_to_server(newLocation, fileName, login, password)
{
  try
  {
    var contentsStream = output_current_editor_to_stream();
    if (!contentsStream)
    {
      dump("failed to get a stream\n");
      return;
    }

    var ftpChannel = get_destination_channel(newLocation, fileName, login, password);
    if (!ftpChannel)
    {
      dump("failed to get a destination channel\n");
      return;
    }
    ftpChannel.uploadStream = contentsStream;
    ftpChannel.asyncOpen(null, null);
    dump("done\n");
  }
  catch (e)
  {
    dump("an error occurred: " + e + "\n");
  }
}

// this function takes a full url, creates an nsIURI, and then creates a channel from that nsIURI
function create_channel_from_url(ioService, aURL, aLogin, aPassword)
{
  try
  {
    var nsiuri = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURI);
    if (!nsiuri)
      return null;
    nsiuri.spec = aURL;
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
    dump(e+"\n");
    return null;
  }
}

function PublishCopyFile(srcDirectoryLocation, destinationDirectoryLocation, fileName, aLogin, aPassword)
{
  // append '/' if it's not there; inputs should be directories so should end in '/'
  if ( srcDirectoryLocation.charAt(srcDirectoryLocation.length-1) != '/' )
    srcDirectoryLocation = srcDirectoryLocation + '/';
  if ( destinationDirectoryLocation.charAt(destinationDirectoryLocation.length-1) != '/' )
    destinationDirectoryLocation = destinationDirectoryLocation + '/';

  try
  {
    // grab an io service
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    if (!ioService)
    {
      dump("failed to get io service\n");
      return;
    }

    // create a channel for the source location
    srcChannel = create_channel_from_url(ioService, srcDirectoryLocation + fileName, null, null);
    if (!srcChannel)
    {
      dump("can't create src channel\n");
      return;
    }

    // create a channel for the destination location
    var ftpChannel = get_destination_channel(destinationDirectoryLocation, fileName, aLogin, aPassword);
    if (!ftpChannel)
    {
      dump("failed to get ftp channel\n");
      return;
    }
    ftpChannel.uploadStream = srcChannel.open();
    ftpChannel.asyncOpen(null, null);
    dump("done\n");
  }
  catch (e)
  {
    dump("an error occurred: " + e + "\n");
  }
}