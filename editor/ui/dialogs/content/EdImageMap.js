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
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Dan Haddix (dan6992@hotmail.com)
 *   Brian King (briano9@yahoo.com)
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

var tHide = false;
var highCont = false;
var imageElement = null;
var mapName = '';
var imageMap = null;
var imageEl;
var srcInputValue = null;
var marquee = null;
var frameDoc = null;
var buttonArray = new Array();

function Startup(){
  if (!InitEditorShell())
    return;
  doSetOKCancel(finishMap, null);
  initDialog();
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?imagemap_properties");
}

function initDialog(){
  //Get image element from parent
  imageElement = window.arguments[0];
  if (!imageElement) //If not an image close window
  {
    window.close();
    return;
  }

  //Get image map from parent
  imageMap = window.arguments[1];
  if (!imageMap) //If no image map close window
    window.close();

  //find parent inputs
  var srcInput = window.opener.document.getElementById("srcInput");
  var widthInput = window.opener.document.getElementById("widthInput");
  var heightInput = window.opener.document.getElementById("heightInput");

  //check for relative url
  if (!((srcInput.value.indexOf("http://") != -1) || (srcInput.value.indexOf("file://") != -1))){
    if (editorShell.editorDocument.location == "about:blank"){
      alert(GetString("ImapRelative"));
      window.close();
      //TODO: add option to save document now
    }
    else{
      var edDoc = new String(editorShell.editorDocument.location);
      var imgDoc = new String(srcInput.value);
      imgDoc = imgDoc.split("../");
      var len = imgDoc.length;
      for (var i=0; i<len; i++){
        if (edDoc.length > (String(editorShell.editorDocument.location.protocol).length+2))
          edDoc = edDoc.substring(0, edDoc.lastIndexOf("/"));
      }
      imgDoc = edDoc+"/"+imgDoc[imgDoc.length-1];
      srcInputValue = imgDoc;
    }
  }
  else{
    srcInputValue = srcInput.value;
  }

  //Set iframe pointer
  frameDoc = window.frames[0].document;

  //Fill button array
  buttonArray[0] = document.getElementById("pointerButton");
  buttonArray[1] = document.getElementById("rectButton");
  buttonArray[2] = document.getElementById("cirButton");
  buttonArray[3] = document.getElementById("polyButton");

  //Create marquee
  var marquee = frameDoc.createElement("div");
  marquee.setAttribute("id", "marquee");
  frameDoc.body.appendChild(marquee);

  //Create background div
  var bgDiv = frameDoc.createElement("div");
  if ( bgDiv ) {
    bgDiv.setAttribute("id", "bgDiv");
    frameDoc.body.appendChild(bgDiv);
  }

  //Place Image
  var newImg = frameDoc.createElement("img");
  if ( newImg ) {
    newImg.setAttribute("src", srcInputValue);
    if (parseInt(widthInput.value) > 0)
      newImg.setAttribute("width", widthInput.value);
    if (parseInt(heightInput.value) > 0)
      newImg.setAttribute("height", heightInput.value);
    newImg.setAttribute("id", "mainImg");
    imageEl = frameDoc.getElementById("bgDiv").appendChild(newImg);
    imageEl.addEventListener("error", imgError, false);
  }

  //Resize background DIV to fit image
  fixBgDiv();

  //Recreate Image Map if it exists
  recreateMap();
}

function imgError(){
  alert(GetString("ImapError")+" " + srcInputValue+"."+GetString("ImapCheck"));
}

function fixBgDiv(){
  imageEl = frameDoc.getElementById("mainImg");
  if (imageEl.offsetWidth != 0){
    frameDoc.getElementById("bgDiv").style.width = imageEl.offsetWidth;
    frameDoc.getElementById("bgDiv").style.height = imageEl.offsetHeight;
  }
  else
    setTimeout("fixBgDiv()", 100);
}

function hideToolbar(){
  // Check to see if toolbar is already hidden
  if (tHide){
    // If it is show it
    document.getElementById("toolbar").setAttribute("collapsed", "false");
    // Set the menu items text back to "Hide Toolbar"
    document.getElementById("view_hidetoolbar").setAttribute("label", GetString("HideToolbar"));
    tHide = false
  }
  else{
    // If not hide it
    document.getElementById("toolbar").setAttribute("collapsed", "true");
    //Set the menu items text to "Show Toolbar"
    document.getElementById("view_hidetoolbar").setAttribute("label", GetString("ShowToolbar"));
    tHide = true;
  }
}

function highContrast(){
  if (highCont == true){
    frameDoc.getElementById("bgDiv").style.background = "url('chrome://editor/skin/images/Map_checker.gif')";
    frameDoc.getElementById("bgDiv").style.backgroundColor = "white";
    imageEl.style.setProperty("-moz-opacity", "1.0", true);
    document.getElementById("Map:Contrast").setAttribute("checked", "false");
    document.getElementById("Map:Contrast").setAttribute("toggled", "false");
    highCont = false;
  }
  else{
    frameDoc.getElementById("bgDiv").style.background = "url('')";
    frameDoc.getElementById("bgDiv").style.backgroundColor = "#D2D2D2";
    imageEl.style.setProperty("-moz-opacity", ".3", true);
    document.getElementById("Map:Contrast").setAttribute("checked", "true");
    document.getElementById("Map:Contrast").setAttribute("toggled", "true");
    highCont = true;
  }
}

function recreateMap(){
  var areaCollection = imageMap.childNodes;
  var areaColLen = areaCollection.length;
  for(var j=0; j<areaColLen; j++){
      area = areaCollection[j];
      shape = area.getAttribute("shape");
      shape = shape.toLowerCase();
      coords = area.getAttribute("coords");
      href = area.getAttribute("href");
      target = area.getAttribute("target");
      alt = area.getAttribute("alt");
      if (shape == "rect")
        Rect(coords, href, target, alt, true);
      else if (shape == "circle")
        Circle(coords, href, target, alt, true);
      else
        Poly(coords, href, target, alt, true);
    }
}

function finishMap(){
  if (!setMapName())
    return false;
  if (!deleteAreas())
    return false;

  spots = frameDoc.getElementsByName("hotspot");
  var len = spots.length;
  if (len >= 1){
    for(i=0; i<len; i++){
      dump(i+"\n");
      curSpot = spots[i];
      if (curSpot.getAttribute("class") == "rect")
        createRect(curSpot);
      else if (curSpot.getAttribute("class") == "cir")
        createCir(curSpot);
      else
        createPoly(curSpot);
    }
    //editorShell.editorDocument.body.appendChild(imageMap);
    //returnValue = "test";
    //window.arguments[0] = "test"; //editorShell.editorDocument.body.appendChild(imageMap); //editorShell.InsertElementAtSelection(imageMap, false);
    //dump(window.arguments[0]+"\n");
    dump("imageMap.childNodes.length = "+imageMap.childNodes.length+"\n");
  }
  return true;
}

function setMapName(){
  //imageMap = editorShell.CreateElementWithDefaults("map");
  //dump(imageMap+"\n");
  //imageMap = frameDoc.createElement("map");

  mapName = imageMap.getAttribute("name");
  if (mapName == ""){
    mapName = String(frameDoc.getElementById("mainImg").getAttribute("src"));
  mapName = mapName.substring(mapName.lastIndexOf("/"), mapName.length);
  mapName = mapName.substring(mapName.lastIndexOf("\\"), mapName.length);
    mapName = mapName.substring(1, mapName.lastIndexOf("."));
    if (mapName == ""){
      // BUG causes substring to return nothing when
      // parameters are 1 & 13 (i.e. string.substring(1, 13);)
      mapName = "hack";
  }
  imageMap.setAttribute("name", mapName);
}
  return true;
}

function createRect(which){
  //newRect = editorShell.CreateElementWithDefaults("area");
  newRect = frameDoc.createElement("area");
  newRect.setAttribute("shape", "rect");
  coords = parseInt(which.style.left)+","+parseInt(which.style.top)+","+(parseInt(which.style.left)+parseInt(which.style.width))+","+(parseInt(which.style.top)+parseInt(which.style.height));
  newRect.setAttribute("coords", coords);
  if (which.getAttribute("hsHref") != ""){
    newRect.setAttribute("href", which.getAttribute("hsHref"));
  }
  else{
    newRect.setAttribute("nohref", "");
  }
  if (which.getAttribute("hsTarget") != ""){
  newRect.setAttribute("target", which.getAttribute("hsTarget"));
  }
  if (which.getAttribute("hsAlt") != ""){
  newRect.setAttribute("alt", which.getAttribute("hsAlt"));
  }
  //newRect.removeAttribute("id");
  imageMap.appendChild(newRect);
}

function createCir(which){
  //newCir = editorShell.CreateElementWithDefaults("area");
  var newCir = frameDoc.createElement("area");
  if ( !newCir )
    return;

  newCir.setAttribute("shape", "circle");
  radius = Math.floor(parseInt(which.style.width)/2);
  coords = (parseInt(which.style.left)+radius)+","+(parseInt(which.style.top)+radius)+","+radius;
  newCir.setAttribute("coords", coords);
  if (which.getAttribute("hsHref") != "")
    newCir.setAttribute("href", which.getAttribute("hsHref"));
  else{
    newCir.setAttribute("nohref", "");
  }
  if (which.getAttribute("hsTarget") != ""){
    newCir.setAttribute("target", which.getAttribute("hsTarget"));
  }
  if (which.getAttribute("hsAlt") != ""){
    newCir.setAttribute("alt", which.getAttribute("hsAlt"));
  }
  //newCir.removeAttribute("id");
  imageMap.appendChild(newCir);
}

function createPoly(which){
  //newPoly = editorShell.CreateElementWithDefaults("area");
  var newPoly = frameDoc.createElement("area");
  if ( !newPoly )
    return;

  newPoly.setAttribute("shape", "poly");
  var coords = '';
  var len = which.childNodes.length;
  for(l=0; l<len; l++){
    coords += (parseInt(which.style.left)+parseInt(which.childNodes[l].style.left))+","+(parseInt(which.style.top)+parseInt(which.childNodes[l].style.top))+",";
  }
  coords = coords.substring(0, (coords.length-1));
  newPoly.setAttribute("coords", coords);
  if (which.getAttribute("hsHref") != "")
    newPoly.setAttribute("href", which.getAttribute("hsHref"));
  else{
    newPoly.setAttribute("nohref", "");
  }
  if (which.getAttribute("hsTarget") != ""){
    newPoly.setAttribute("target", which.getAttribute("hsTarget"));
  }
  if (which.getAttribute("hsAlt") != ""){
    newPoly.setAttribute("alt", which.getAttribute("hsAlt"));
  }
  //newPoly.removeAttribute("id");
  imageMap.appendChild(newPoly);
}

function hotSpotProps(which){
  var currentRect = null;
  var currentCir = null;
  if (which == null)
    return;
  hotSpotWin = window.openDialog("chrome://editor/content/EdImageMapHotSpot.xul", "_blank", "chrome,close,titlebar,modal", which);
}

function deleteAreas(){
  dump("deleteAreas called\n");
  area = imageMap.firstChild;
  while (area != null){
    dump(area+"\n");
    imageMap.removeChild(area);
    area = imageMap.firstChild;
  }
  return true;
}

// This is contained in editor.js (should be in a common js file
// I did not want to include the whole file so I copied the function here
// It retrieves strings from editor string bundle
function GetString(id)
{
  return editorShell.GetString(id);
}
