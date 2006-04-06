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
 * The Original Code is Mozilla Composer.
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

var gIOService = null;

function NormalizeURL(url)
{
  if (!GetScheme(url) && !IsUrlAboutBlank(url))
  {
    var k = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);

    var noBackSlashUrl = url.replace(/\\\"/g, "\"");
    var c0 = noBackSlashUrl[0];
    var c1 = noBackSlashUrl[1]
    if (c0 == '/' ||
        ( ((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z')) && c1 == ":"))
    {
      // this is an absolute path
      k.initWithPath(url);
    }
    else
    {
      // First, get the current dir for the process...
      var dirServiceProvider = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIDirectoryServiceProvider);
      var p = new Object();
      var currentProcessDir = dirServiceProvider.getFile("CurWorkD", p).path;
      k.initWithPath(currentProcessDir);

      // then try to append the relative path
      try {
        k.appendRelativePath(url);
      }
      catch (e) {
        dump("### Can't understand the filepath\n");
        return "about:blank";
      }
      var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
      url = fileHandler.getURLSpecFromFile(k);
    }
  }
  return url;
}

function IsUrlAboutBlank(urlString)
{
  return (urlString == "about:blank");
}

function GetScheme(urlspec)
{
  var resultUrl = TrimString(urlspec);
  // Unsaved document URL has no acceptable scheme yet
  if (!resultUrl || IsUrlAboutBlank(resultUrl))
    return "";

  var IOService = GetIOService();
  if (!IOService)
    return "";

  var scheme = "";
  try {
    // This fails if there's no scheme
    scheme = IOService.extractScheme(resultUrl);
  } catch (e) {}

  return scheme ? scheme.toLowerCase() : "";
}

function GetIOService()
{
  if (gIOService)
    return gIOService;

  gIOService = Components.classes["@mozilla.org/network/io-service;1"]
               .getService(Components.interfaces.nsIIOService);

  return gIOService;
}

function NewURI(urlstring)
{
  try {
    return GetIOService().newURI(urlstring, null, null);
  } catch (e) {}

  return null;
}

