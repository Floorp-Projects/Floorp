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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Dan Haddix (dan6992@hotmail.com)
 */


var tHide = false;
var highCont = false;
var imageElement;
var mapName;
var imageMap;
var oldMap = false;

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
    window.close();

  //Set iframe pointer
  frameDoc = window.frames[0].document;

  //Fill button array
  buttonArray[0] = document.getElementById("pointerButton");
  buttonArray[1] = document.getElementById("rectButton");
  buttonArray[2] = document.getElementById("cirButton");
  buttonArray[3] = document.getElementById("polyButton");

  //Create marquee
  marquee = frameDoc.createElement("div");
  marquee.setAttribute("id", "marquee");
  frameDoc.body.appendChild(marquee);

  //Create background div
  bgDiv = frameDoc.createElement("div");
  bgDiv.setAttribute("id", "bgDiv");
  frameDoc.body.appendChild(bgDiv);

  //Place Image
  newImg = frameDoc.createElement("img");
  newImg.setAttribute("src", imageElement.getAttribute("src"));
  newImg.setAttribute("width", imageElement.offsetWidth);
  newImg.setAttribute("height", imageElement.offsetHeight);
  newImg.setAttribute("id", "mainImg");
  frameDoc.getElementById("bgDiv").appendChild(newImg);
  frameDoc.getElementById("bgDiv").style.width = imageElement.offsetWidth;

  //Recreate Image Map if it exists
  if (imageElement.getAttribute("usemap") != "")
    recreateMap();

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
  imgEl = frameDoc.getElementById("mainImg");
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
  map = imageElement.getAttribute("usemap");
  map = map.substring(1, map.length);
  mapCollection = imageElement.ownerDocument.getElementsByName(map);
  if (mapCollection){
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
    imageElement.ownerDocument.removeChild(mapCollection[0]);
  }
}

function finishMap(){
  spots = frameDoc.getElementsByName("hotspot");
  var len = spots.length;
  createMap();
  if (len >= 1){
    for(i=0; i<len; i++){
      curSpot = spots[i];
      if (curSpot.getAttribute("class") == "rect")
        createRect(curSpot);
      else if (curSpot.getAttribute("class") == "cir")
        createCir(curSpot);
      else
        createPoly(curSpot);
    }
    imageElement.setAttribute("usemap", ("#"+mapName));
//    alert(imageMap);
    editorShell.InsertElementAtSelection(imageMap, false);
    dump("image map element inserted\n");
  }
  return true;
}

function createMap(){
  imageMap = editorShell.CreateElementWithDefaults("map");
  mapName = imageElement.getAttribute("src");
  mapName = mapName.substring(mapName.lastIndexOf("/"), mapName.length);
  mapName = mapName.substring(mapName.lastIndexOf("\\"), mapName.length);
  mapName = mapName.substring(1, mapName.indexOf("."));
  imageMap.setAttribute("name", mapName);
}

function createRect(which){
  newRect = document.createElement("area");
  newRect.setAttribute("shape", "rect");
  coords = parseInt(which.style.left)+","+parseInt(which.style.top)+","+(parseInt(which.style.left)+parseInt(which.style.width))+","+(parseInt(which.style.top)+parseInt(which.style.height));
  newRect.setAttribute("coords", coords);
  if (which.getAttribute("hsHref") != ""){
    newRect.setAttribute("href", which.getAttribute("hsHref"));
  }
  else{
    newRect.setAttribute("nohref", "");
  }
  newRect.setAttribute("target", which.getAttribute("hsTarget"));
  newRect.setAttribute("alt", which.getAttribute("hsAlt"));
  newRect.removeAttribute("id");
  imageMap.appendChild(newRect);
}

function createCir(which){
  newCir = document.createElement("area");
  newCir.setAttribute("shape", "circle");
  radius = Math.floor(parseInt(which.style.width)/2);
  coords = (parseInt(which.style.left)+radius)+","+(parseInt(which.style.top)+radius)+","+radius;
  newCir.setAttribute("coords", coords);
  if (which.getAttribute("hsHref") != "")
    newCir.setAttribute("href", which.getAttribute("hsHref"));
  else{
    newCir.setAttribute("nohref", "");
  }
  newCir.setAttribute("target", which.getAttribute("hsTarget"));
  newCir.setAttribute("alt", which.getAttribute("hsAlt"));
  newCir.removeAttribute("id");
  imageMap.appendChild(newCir);
}

function createPoly(which){
  newPoly = document.createElement("area");
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
  newPoly.setAttribute("target", which.getAttribute("hsTarget"));
  newPoly.setAttribute("alt", which.getAttribute("hsAlt"));
  newPoly.removeAttribute("id");
  imageMap.appendChild(newPoly);
}

function hotSpotProps(which){
  currentRect = null;
  currentCir = null;
  if (which == null)
    return;
  hotSpotWin = window.openDialog("chrome://editor/content/EdImageMapHotSpot.xul", "_blank", "chrome,close,titlebar,modal", which);
}

function deleteAreas(){
  if (oldMap){
    area = imageMap.firstChild;
    while (area){
      area = imageMap.removeChild(area).nextSibling;
    }    
  }
}