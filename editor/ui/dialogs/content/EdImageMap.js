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
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Dan Haddix (dan6992@hotmail.com)
 *   Brian King
 */


var tHide = false;
var highCont = false;
var imageElement = null;
var mapName = '';
var imageMap = null;
var oldMap = null;
var noImg = false;

function Startup(){
  if (!InitEditorShell())
    return;
  doSetOKCancel(finishMap, null);
  initDialog();
}

function initDialog(){
  //Check to make sure selected element is an image
  imageElement = editorShell.GetSelectedElement("img");
  if (!imageElement) //If not an image close window
  {
    imageElementName = window.opener.document.getElementById('srcInput');
    imageElementName = imageElementName.value;
    if ( IsValidImage(imageElementName )) {
      noImg = true;
    }
    else {
      // Need error message here
      window.close();
      return;
    } 
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
  if ( newImg && noImg == false) {
    newImg.setAttribute("src", imageElement.getAttribute("src"));
    newImg.setAttribute("width", imageElement.offsetWidth);
    newImg.setAttribute("height", imageElement.offsetHeight);
    newImg.setAttribute("id", "mainImg");
    frameDoc.getElementById("bgDiv").appendChild(newImg);
    frameDoc.getElementById("bgDiv").style.width = imageElement.offsetWidth;
  }
  else if (newImg) {
    newImg.setAttribute("src", imageElementName);
    frameDoc.getElementById("bgDiv").appendChild(newImg);
  }

  //Recreate Image Map if it exists
  if ( (noImg == false) && (imageElement.getAttribute("usemap")) ) {
    //alert('test');
    recreateMap();
  }
}

function hideToolbar(){
  // Check to see if toolbar is already hidden
  if (tHide){
    // If it is show it
    document.getElementById("toolbar").setAttribute("collapsed", "false");
    // Set the menu items text back to "Hide Toolbar"
    document.getElementById("view_hidetoolbar").setAttribute("value", "Hide Toolbar");
    tHide = false
  }
  else{
    // If not hide it
    document.getElementById("toolbar").setAttribute("collapsed", "true");
    //Set the menu items text to "Show Toolbar"
    document.getElementById("view_hidetoolbar").setAttribute("value", "Show Toolbar");
    tHide = true;
  }
}

function highContrast(){
  var imgEl = frameDoc.getElementById("mainImg");
  if (highCont){
    imgEl.style.opacity = "100%";
    highCont = false;
  }
  else{
    imgEl.style.opacity = "10%";
    highCont = true;
  }
}

function recreateMap(){
  var map = imageElement.getAttribute("usemap");
  map = map.substring(1, map.length);
  mapName = map;
  var mapCollection = editorShell.editorDocument.getElementsByName(map);
  oldMap = mapCollection[0];
  //alert(map);
  try{
    alert(oldMap.childNodes.length);
  }
  catch (ex){
    alert(ex);
  }
    areaCollection = mapCollection[0].childNodes;
    var len = areaCollection.length;
    for(j=0; j<len; j++){
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
  var spots = frameDoc.getElementsByName("hotspot");
  var len = spots.length;
  createMap();
  if (len >= 1){
    for(i=0; i<len; i++){
      var curSpot = spots[i];
      if (curSpot.getAttribute("class") == "rect")
        createRect(curSpot);
      else if (curSpot.getAttribute("class") == "cir")
        createCir(curSpot);
      else
        createPoly(curSpot);
    }
    imageElement.setAttribute("usemap", ("#"+mapName));
    editorShell.editorDocument.body.appendChild(imageMap);
    //editorShell.InsertElementAtSelection(imageMap, false);
  }
  return true;
}

function createMap(){
  //imageMap = editorShell.CreateElementWithDefaults("map");
  imageMap = frameDoc.createElement("map");
  if (mapName == ''){
  mapName = imageElement.getAttribute("src");
  mapName = mapName.substring(mapName.lastIndexOf("/"), mapName.length);
  mapName = mapName.substring(mapName.lastIndexOf("\\"), mapName.length);
  mapName = mapName.substring(1, mapName.indexOf("."));
  }
  else {
    editorShell.editorDocument.body.removeChild(oldMap);
  }
  imageMap.setAttribute("name", mapName);
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
  if (which.getAttribute("target") != ""){
  newRect.setAttribute("target", which.getAttribute("hsTarget"));
  }
  if (which.getAttribute("alt") != ""){
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

  if ( which.getAttribute("hsTarget") )
    newCir.setAttribute("target", which.getAttribute("hsTarget"));
  if ( which.getAttribute("hsAlt") )
    newCir.setAttribute("alt", which.getAttribute("hsAlt"));
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
  if ( which.getAttribute("hsTarget") )
    newPoly.setAttribute("target", which.getAttribute("hsTarget"));
  if ( which.getAttribute("hsAlt") )
    newPoly.setAttribute("alt", which.getAttribute("hsAlt"));
  //newPoly.removeAttribute("id");
  imageMap.appendChild(newPoly);
}

function hotSpotProps(which){
  var currentRect = null;
  var currentCir = null;
  if (which == null)
    return;
  var hotSpotWin = window.openDialog("chrome://editor/content/EdImageMapHotSpot.xul", "_blank", "chrome,close,titlebar,modal", which);
}

function deleteAreas(){
  if (oldMap){
    area = imageMap.firstChild;
    while (area){
      area = imageMap.removeChild(area).nextSibling;
    }    
  }
}