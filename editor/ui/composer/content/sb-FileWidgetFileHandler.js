/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Ben Goodger
 */

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