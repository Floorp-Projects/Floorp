/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

function fillContextMenu(name,node)
{
    if (!name)    return(false);
    var popupNode = document.getElementById(name);
    if (!popupNode)    return(false);

    var url = GetFileURL(node);             // get the URL of the selected file
    var ext = getFileExtension(url);        // get the extension (type) of file

    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
    var menuNode = popupNode.childNodes[0];
    popupNode.removeChild(menuNode);

    // create a new menu node
    menuNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menu");
    popupNode.appendChild(menuNode);

    switch(ext) {
    case "gif":
    case "jpg":
    case "jpeg":
    case "png":
        isImageFile(menuNode,url);
        break;
    case "htm":
    case "html":
        isHTMLFile(menuNode,url);
        break;
    default:
        break;
    }

    return(true);
}

function GetFileURL(node)
{
    var url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
    {
        return(false);
    }

  try
  {
    // add support for IE favorites under Win32, and NetPositive URLs under BeOS
    if (url.indexOf("file://") == 0)
    {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
      if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
      if (rdf)
      {
        var fileSys = rdf.GetDataSource("rdf:files");
        if (fileSys)
        {
          var src = rdf.GetResource(url, true);
          var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
          var target = fileSys.GetTarget(src, prop, true);
          if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (target) target = target.Value;
          if (target) url = target;

        }
      }
    }
  }
  catch(ex)
  {
  }
    return url;
}

function isImageFile(parent,url)
{
    // note: deleted all the doContextCmd stuff from bookmarks.
    var menuItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menuitem");
    menuItem.setAttribute("label","Insert Image");
    // menuItem.setAttribute("onaction","AutoInsertImage(\'" + url + "\')");
    parent.appendChild(menuItem);
}
