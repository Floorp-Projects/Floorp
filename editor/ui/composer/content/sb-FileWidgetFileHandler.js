/*  */
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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
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

function EditorAutoInsertImage(url)
{
    var imageElement = top.editorShell.CreateElementWithDefaults("img");
    // TODO: do some smartass string parsing to find relative file
    //       location so we don't need this big chunky URLs.. 
    //       must utilize document dirty flag.
    imageElement.setAttribute("src",url);
    // TODO: make this show the file name and size in bytes. 
    var altString = url.substring(url.lastIndexOf("/")+1,url.length);
    imageElement.setAttribute("alt",altString);
    imageElement.setAttribute("border","0");
    // TODO: collect width and height attributes and insert those
    //       also.
    // insert the image
    try {
      top.editorShell.InsertElementAtSelection(imageElement,true);
    } catch (e) {
      dump("Exception occured in InsertElementAtSelection\n");
    }
}

function EditorInsertBackgroundImage(url)
{
    
}

function EditorInsertJSFile(url)
{
    var scriptElement = top.editorShell.CreateElementWithDefaults("script");
    // TODO: do some smartass string parsing to find relative file
    //       location so we don't need this big chunky URLs.. 
    //       must utilize document dirty flag.
    scriptElement.setAttribute("src",url);
    scriptElement.setAttribute("language","JavaScript");    
    try {
      top.editorShell.InsertElementAtSelection(scriptElement,true);
    } catch (e) {
      dump("Exception occured in InsertElementAtSelection\n");
    }
}

function EditorInsertCSSFile(url)
{
    var cssElement = top.editorShell.CreateElementWithDefaults("link");
    // TODO: do some smartass string parsing to find relative file
    //       location so we don't need this big chunky URLs.. 
    //       must utilize document dirty flag.
    cssElement.setAttribute("href",url);
    cssElement.setAttribute("type","text/css");    
    cssElement.setAttribute("rel","stylesheet");    
    try {
      top.editorShell.InsertElementAtSelection(cssElement,true);
    } catch (e) {
      dump("Exception occured in InsertElementAtSelection\n");
    }
}