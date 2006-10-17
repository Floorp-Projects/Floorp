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
 * The Original Code is Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
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

var UrlUtils = {

  /********** ATTRIBUTES **********/

  mIOService: null,
  mOS: null,

  /********** PUBLIC **********/

  normalizeURL: function normalizeURL(url)
  {
    if (!this.getScheme(url) && !this.isUrlAboutBlank(url))
    {
      var k = Components.classes["@mozilla.org/file/local;1"]
                        .createInstance(Components.interfaces.nsILocalFile);

      var noBackSlashUrl = url.replace(/\\\"/g, "\"");
      var c0 = noBackSlashUrl[0];
      var c1 = noBackSlashUrl[1];
      if ((c0 == '/') || ((/^[a-zA-z]/.test(c0)) && (c1 == ":") ))
      {
        // this is an absolute path
        k.initWithPath(url);
      }
      else
      {
        // First, get the current dir for the process...
        var dirServiceProvider =
          Components.classes["@mozilla.org/file/directory_service;1"]
                    .getService(Components.interfaces.nsIDirectoryServiceProvider);
        var p = {};
        var currentProcessDir = dirServiceProvider.getFile("CurWorkD", p).path;
        k.initWithPath(currentProcessDir);

        // then try to append the relative path
        try {
          k.appendRelativePath(url);
        }
        catch (e) {
          dump("### Can't understand the filepath\n");
          return "chrome://composer/content/blanks/transitional.html";
        }
        var ioService =
          Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
        var fileHandler =
          ioService.getProtocolHandler("file")
                   .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        url = fileHandler.getURLSpecFromFile(k);
      }
    }
    return url;
  },

  isUrlAboutBlank: function isUrlAboutBlank(urlString)
  {
    return (urlString == "chrome://composer/content/blanks/transitional.html");
  },

  getScheme: function getScheme(urlspec)
  {
    var resultUrl = TrimString(urlspec);
    // Unsaved document URL has no acceptable scheme yet
    if (!resultUrl || this.isUrlAboutBlank(resultUrl))
      return "";

    var IOService = this.getIOService();
    if (!IOService)
      return "";

    var scheme = "";
    try {
      // This fails if there's no scheme
      scheme = IOService.extractScheme(resultUrl);
    } catch (e) { dump (e)+"\n"; }

    return scheme ? scheme.toLowerCase() : "";
  },

  getIOService: function getIOService()
  {
    if (this.mIOService)
      return this.mIOService;

    this.mIOService = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);

    return this.mIOService;
  },

  newURI: function newURI(aURLString)
  {
    try {
      return this.getIOService().newURI(aURLString, null, null);
    } catch (e) { dump (e)+"\n"; }

    return null;
  },

  isTextURI: function isTextURI(aText)
  {
    return aText && /^http:\/\/|^https:\/\/|^file:\/\/|\
      ^ftp:\/\/|^about:|^mailto:|^news:|^snews:|^telnet:|^ldap:|\
      ^ldaps:|^gopher:|^finger:|^javascript:/i.test(aText);
  },

  makeRelativeUrl: function makeRelativeUrl(aURLString)
  {
    var inputUrl = TrimString(aURLString);
    if (!inputUrl)
      return inputUrl;

    // Get the filespec relative to current document's location
    // NOTE: Can't do this if file isn't saved yet!
    var docUrl = this.getDocumentBaseUrl();
    var docScheme = this.getScheme(docUrl);

    // Can't relativize if no doc scheme (page hasn't been saved)
    if (!docScheme)
      return inputUrl;

    var urlScheme = this.getScheme(inputUrl);

    // Do nothing if not the same scheme or url is already relativized
    if (docScheme != urlScheme)
      return inputUrl;

    var IOService = this.getIOService();
    if (!IOService)
      return inputUrl;

    // Host must be the same
    var docHost = this.getHost(docUrl);
    var urlHost = this.getHost(inputUrl);
    if (docHost != urlHost)
      return inputUrl;


    // Get just the file path part of the urls
    var charset = EditorUtils.getCurrentEditor().documentCharacterSet;
    var docPath = IOService.newURI(docUrl,   charset, null).path;
    var urlPath = IOService.newURI(inputUrl, charset, null).path;

    // We only return "urlPath", so we can convert
    //  the entire docPath for case-insensitive comparisons
    var os = this.getOS();
    var doCaseInsensitive = (docScheme == "file" && os == gWin);
    if (doCaseInsensitive)
      docPath = docPath.toLowerCase();

    // Get document filename before we start chopping up the docPath
    var docFilename = this.getFilename(docPath);

    // Both url and doc paths now begin with "/"
    // Look for shared dirs starting after that
    urlPath = urlPath.slice(1);
    docPath = docPath.slice(1);

    var firstDirTest = true;
    var nextDocSlash = 0;
    var done = false;

    // Remove all matching subdirs common to both doc and input urls
    do {
      nextDocSlash = docPath.indexOf("\/");
      var nextUrlSlash = urlPath.indexOf("\/");

      if (nextUrlSlash == -1)
      {
        // We're done matching and all dirs in url
        // what's left is the filename
        done = true;

        // Remove filename for named anchors in the same file
        if (nextDocSlash == -1 && docFilename)
        { 
          var anchorIndex = urlPath.indexOf("#");
          if (anchorIndex > 0)
          {
            var urlFilename = doCaseInsensitive ? urlPath.toLowerCase() : urlPath;
          
            if (urlFilename.indexOf(docFilename) == 0)
              urlPath = urlPath.slice(anchorIndex);
          }
        }
      }
      else if (nextDocSlash >= 0)
      {
        // Test for matching subdir
        var docDir = docPath.slice(0, nextDocSlash);
        var urlDir = urlPath.slice(0, nextUrlSlash);
        if (doCaseInsensitive)
          urlDir = urlDir.toLowerCase();

        if (urlDir == docDir)
        {

          // Remove matching dir+"/" from each path
          //  and continue to next dir
          docPath = docPath.slice(nextDocSlash+1);
          urlPath = urlPath.slice(nextUrlSlash+1);
        }
        else
        {
          // No match, we're done
          done = true;

          // Be sure we are on the same local drive or volume 
          //   (the first "dir" in the path) because we can't 
          //   relativize to different drives/volumes.
          // UNIX doesn't have volumes, so we must not do this else
          //  the first directory will be misinterpreted as a volume name
          if (firstDirTest && docScheme == "file" && os != gUNIX)
            return inputUrl;
        }
      }
      else  // No more doc dirs left, we're done
        done = true;

      firstDirTest = false;
    }
    while (!done);

    // Add "../" for each dir left in docPath
    while (nextDocSlash > 0)
    {
      urlPath = "../" + urlPath;
      nextDocSlash = docPath.indexOf("\/", nextDocSlash + 1);
    }
    return urlPath;
  },

  makeAbsoluteUrl: function makeAbsoluteUrl(url)
  {
    var resultUrl = TrimString(url);
    if (!resultUrl)
      return resultUrl;

    // Check if URL is already absolute, i.e., it has a scheme
    var urlScheme = this.getScheme(resultUrl);

    if (urlScheme)
      return resultUrl;

    var docUrl = this.getDocumentBaseUrl();
    var docScheme = this.getScheme(docUrl);

    // Can't relativize if no doc scheme (page hasn't been saved)
    if (!docScheme)
      return resultUrl;

    var  IOService = this.getIOService();
    if (!IOService)
      return resultUrl;
    
    // Make a URI object to use its "resolve" method
    var absoluteUrl = resultUrl;
    var docUri = IOService.newURI(docUrl, EditorUtils.getCurrentEditor().documentCharacterSet, null);

    try {
      absoluteUrl = docUri.resolve(resultUrl);
      // This is deprecated and buggy! 
      // If used, we must make it a path for the parent directory (remove filename)
      //absoluteUrl = IOService.resolveRelativePath(resultUrl, docUrl);
    } catch (e) { dump (e)+"\n"; }

    return absoluteUrl;
  },

  getDocumentBaseUrl: function getDocumentBaseUrl()
  {
    try {
      var docUrl;

      // if document supplies a <base> tag, use that URL instead 
      var baseList = EditorUtils.getCurrentEditor().document.getElementsByTagName("base");
      if (baseList)
      {
        var base = baseList.item(0);
        if (base)
          docUrl = base.getAttribute("href");
      }
      if (!docUrl)
        docUrl = this.getDocumentUrl();

      if (!this.isUrlAboutBlank(docUrl))
        return docUrl;
    } catch (e) { dump (e)+"\n"; }
    return "";
  },

  getDocumentUrl: function getDocumentUrl()
  {
    try {
      var aDOMHTMLDoc = EditorUtils.getCurrentEditor().document.QueryInterface(Components.interfaces.nsIDOMHTMLDocument);
      return aDOMHTMLDoc.URL;
    }
    catch (e) { dump (e)+"\n"; }
    return "";
  },

  getScheme: function getScheme(aURLSpec)
  {
    var resultUrl = TrimString(aURLSpec);
    // Unsaved document URL has no acceptable scheme yet
    if (!resultUrl || this.isUrlAboutBlank(resultUrl))
      return "";

    var IOService = this.getIOService();
    if (!IOService)
      return "";

    var scheme = "";
    try {
      // This fails if there's no scheme
      scheme = IOService.extractScheme(resultUrl);
    } catch (e) { dump (e)+"\n"; }

    return scheme ? scheme.toLowerCase() : "";
  },

  _getURI: function _getURI(aURLSpec)
  {
    if (!aURLSpec)
      return "";

    var IOService = this.getIOService();
    if (!IOService)
      return "";

    var uri = null;
    try {
      uri = IOService.newURI(aURLSpec, null, null);
     } catch (e) { dump (e)+"\n"; }

    return uri;
  },

  getHost: function getHost(aURLSpec)
  {
    var host = "";
    var uri = this._getURI(aURLSpec);
    if (uri)
      host = uri.host;
    return host;
  },

  getUsername: function getUsername(aURLSpec)
  {
    var username = "";
    var uri = this._getURI(aURLSpec);
    if (uri)
      username = uri.username;
    return username;
  },

  getFilename: function getFilename(aURLSpec)
  {
    var filename = "";
    var uri = this._getURI(aURLSpec);
    if (uri)
    {
      var url = uri.QueryInterface(Components.interfaces.nsIURL);
      if (url)
        filename = url.fileName;
    }

    return filename ? filename : "";
  },

  stripUsernamePassword: function stripUsernamePassword(aURLSpec, usernameObj, passwordObj)
  {
    var urlspec = TrimString(aURLSpec);
    if (!urlspec || this.isUrlAboutBlank(urlspec))
      return urlspec;

    if (usernameObj)
      usernameObj.value = "";
    if (passwordObj)
      passwordObj.value = "";

    // "@" must exist else we will never detect username or password
    var atIndex = aURLSpec.indexOf("@");
    if (atIndex > 0)
    {
      try {
        var IOService = this.getIOService();
        if (!IOService)
          return urlspec;

        var uri = IOService.newURI(urlspec, null, null);
        var username = uri.username;
        var password = uri.password;

        if (usernameObj && username)
          usernameObj.value = username;
        if (passwordObj && password)
          passwordObj.value = password;
        if (username)
        {
          var usernameStart = urlspec.indexOf(username);
          if (usernameStart != -1)
            return urlspec.slice(0, usernameStart) + urlspec.slice(atIndex+1);
        }
      } catch (e) { dump (e)+"\n"; }
    }
    return urlspec;
  },

  stripPassword: function stripPassword(aURLSpec, passwordObj)
  {
    var urlspec = TrimString(aURLSpec);
    if (!urlspec || this.isUrlAboutBlank(urlspec))
      return urlspec;

    if (passwordObj)
      passwordObj.value = "";

    // "@" must exist else we will never detect password
    var atIndex = urlspec.indexOf("@");
    if (atIndex > 0)
    {
      try {
        var IOService = this.getIOService();
        if (!IOService)
          return urlspec;

        var password = IOService.newURI(urlspec, null, null).password;

        if (passwordObj && password)
          passwordObj.value = password;
        if (password)
        {
          // Find last ":" before "@"
          var colon = urlspec.lastIndexOf(":", atIndex);
          if (colon != -1)
          {
            // Include the "@"
            return urlspec.slice(0, colon) + urlspec.slice(atIndex);
          }
        }
      } catch (e) { dump (e)+"\n"; }
    }
    return urlspec;
  },

  // Version to use when you have an nsIURI object
  stripUsernamePasswordFromURI: function stripUsernamePasswordFromURI(aURI)
  {
    var urlspec = "";
    if (aURI)
    {
      try {
        urlspec = aURI.spec;
        var userPass = aURI.userPass;
        if (userPass)
        {
          start = urlspec.indexOf(userPass);
          urlspec = urlspec.slice(0, start) + urlspec.slice(start+userPass.length+1);
        }
      } catch (e) { dump (e)+"\n"; }    
    }
    return urlspec;
  },

  insertUsernameIntoUrl: function insertUsernameIntoUrl(aURLSpec, aUserName)
  {
    if (!aURLSpec || !aUserName)
      return aURLSpec;

    try {
      var ioService = this.getIOService();
      var URI = ioService.newURI(aURLSpec, this.getCurrentEditor().documentCharacterSet, null);
      URI.username = aUserName;
      return URI.spec;
    } catch (e) { dump (e)+"\n"; }

    return aURLSpec;
  },

  getOS: function getOS()
  {
    if (this.mOS)
      return this.mOS;

    var platform = navigator.platform.toLowerCase();

    if (platform.indexOf("win") != -1)
      this.mOS = gWin;
    else if (platform.indexOf("mac") != -1)
      this.mOS = gMac;
    else if (platform.indexOf("unix") != -1 || platform.indexOf("linux") != -1 || platform.indexOf("sun") != -1)
      this.mOS = gUNIX;
    else
      this.mOS = "";
    // Add other tests?

    return this.mOS;
  },

  getFileProtocolHandler: function getFileProtocolHandler()
  {
    try {
      var ios = this.getIOService();
      var handler = ios.getProtocolHandler("file");
      return handler.QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    }
    catch (e) { dump (e)+"\n"; }
    return null;
  }

};


