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
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Dan Haddix (dan6992@hotmail.com)
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

var downTool = false;
var dragActive = false;
var dragObject = false;
var startX = null;
var startY = null;
var endX = null;
var endY = null;
var downTool = false;
var currentElement = new Array();
var currentTool = "pointer";
var currentRect = null;
var currentCir = null;
var currentPoly = null;
var currentPoint = null;
var rectCount = 1;
var cirCount = 1;
var polyCount = 1;
var pointCount = 1;
var xlock = false;
var ylock = false;
var resize = false;
var currentZoom = 1;
var clipBoard = new Array();

function Rect(coords, href, target, alt, construct){
  newRect = frameDoc.createElement("div");
  newRect.setAttribute("class", "rect");
  newRect.setAttribute("id", "rect"+rectCount++);
  newRect.setAttribute("name", "hotspot");
  currentRect = selectElement(frameDoc.body.appendChild(newRect));

  // Add resize handles
  // waiting for better drawing code
  handletl = frameDoc.createElement("div");
  handletl.setAttribute("class", "handletl");
  handletl.setAttribute("name", "handle");
  handletr = frameDoc.createElement("div");
  handletr.setAttribute("class", "handletr");
  handletr.setAttribute("name", "handle");
  handlebl = frameDoc.createElement("div");
  handlebl.setAttribute("class", "handlebl");
  handlebl.setAttribute("name", "handle");
  handlebr = frameDoc.createElement("div");
  handlebr.setAttribute("class", "handlebr");
  handlebr.setAttribute("name", "handle");
  handlet = frameDoc.createElement("div");
  handlet.setAttribute("class", "handlet");
  handlet.setAttribute("name", "handle");
  handlel = frameDoc.createElement("div");
  handlel.setAttribute("class", "handlel");
  handlel.setAttribute("name", "handle");
  handler = frameDoc.createElement("div");
  handler.setAttribute("class", "handler");
  handler.setAttribute("name", "handle");
  handleb = frameDoc.createElement("div");
  handleb.setAttribute("class", "handleb");
  handleb.setAttribute("name", "handle");
  currentRect.appendChild(handletl);
  currentRect.appendChild(handletr);
  currentRect.appendChild(handlebl);
  currentRect.appendChild(handlebr);
  currentRect.appendChild(handlet);
  currentRect.appendChild(handlel);
  currentRect.appendChild(handler);
  currentRect.appendChild(handleb);

  if (!coords){
    currentRect.style.left = startX+"px";
    currentRect.style.top = startY+"px";
    //currentRect.style.width = endX+"px";
    //currentRect.style.height = endX+"px";
  }
  else{
    var coordArray = coords.split(',');
    currentRect.style.left = coordArray[0]+"px";
    currentRect.style.top = coordArray[1]+"px";
    currentRect.style.width = (parseInt(coordArray[2])-parseInt(coordArray[0]))+"px";
    currentRect.style.height = (parseInt(coordArray[3])-parseInt(coordArray[1]))+"px";
    if (href)
      currentRect.setAttribute("hsHref", href);
    if (target)
      currentRect.setAttribute("hsTarget", target);
    if (alt)
      currentRect.setAttribute("hsAlt", alt);
  }
  if (construct)
    currentRect = null;
}

function Circle(coords, href, target, alt, construct){
  newCir = frameDoc.createElement("div");
  newCir.setAttribute("class", "cir");
  newCir.setAttribute("id", "cir"+cirCount++);
  newCir.setAttribute("name", "hotspot");
  currentCir = selectElement(frameDoc.body.appendChild(newCir));

  // Add resize handles
  handletl = frameDoc.createElement("div");
  handletl.setAttribute("class", "handletl");
  handletl.setAttribute("name", "handle");
  handletr = frameDoc.createElement("div");
  handletr.setAttribute("class", "handletr");
  handletr.setAttribute("name", "handle");
  handlebl = frameDoc.createElement("div");
  handlebl.setAttribute("class", "handlebl");
  handlebl.setAttribute("name", "handle");
  handlebr = frameDoc.createElement("div");
  handlebr.setAttribute("class", "handlebr");
  handlebr.setAttribute("name", "handle");
  currentCir.appendChild(handletl);
  currentCir.appendChild(handletr);
  currentCir.appendChild(handlebl);
  currentCir.appendChild(handlebr);

  if (!coords){
    currentCir.style.left = startX+"px";
    currentCir.style.top = startY+"px";
    //currentCir.style.width = endX+"px";
    //currentCir.style.height = endX+"px";
  }
  else{
    var coordArray = coords.split(',');
    radius = parseInt(coordArray[2]);
    currentCir.style.left = (parseInt(coordArray[0])-radius)+"px";
    currentCir.style.top = (parseInt(coordArray[1])-radius)+"px";
    currentCir.style.width = (radius*2)+"px";
    currentCir.style.height = (radius*2)+"px";
    if (href)
      currentCir.setAttribute("hsHref", href);
    if (target)
      currentCir.setAttribute("hsTarget", target);
    if (alt)
      currentCir.setAttribute("hsAlt", alt);
  }
  if (construct)
    currentCir = null;
}

function Poly(coords, href, target, alt, construct){
  dump('Poly Called\n');
  newPoly = frameDoc.createElement("div");
  newPoly.setAttribute("class", "poly");
  newPoly.setAttribute("id", "poly"+polyCount++);
  newPoly.setAttribute("name", "hotspot");
  currentPoly = selectElement(frameDoc.body.appendChild(newPoly));
  if (currentZoom > 1){
    currentPoly.style.width = imageEl.offsetWidth+"px";
    currentPoly.style.height = imageEl.offsetHeight+"px";
  }
  if (!coords){
    addPoint(null, startX, startY, true);
    //currentPoly.onclick = addPoint;
    currentPoly.style.cursor = "crosshair";
  }
  else{
    var coordArray = coords.split(',');
    var len = coordArray.length;
    for (i=0; i<len; i++){
      addPoint(null, coordArray[i], coordArray[i+1]);
      i++;
    }
    if (href)
      currentPoly.setAttribute("hsHref", href);
    if (target)
      currentPoly.setAttribute("hsTarget", target);
    if (alt)
      currentPoly.setAttribute("hsAlt", alt);
    polyFinish(null, construct);
  }
}

function addPoint(event, pointX, pointY, start){
  if (event){
    dump('addPoint Called with event\n');
    pointX = event.clientX+window.frames[0].pageXOffset;
    pointY = event.clientY+window.frames[0].pageYOffset;
    event.preventBubble();
    if (event.detail == 2){
      polyFinish();
      return;
    }
  }
  else
    dump('addPoint Called\n');
  newPoint = frameDoc.createElement("div");
  newPoint.setAttribute("class", "point");
  newPoint.setAttribute("id", "point"+pointCount++);
  newPoint.style.left = pointX+"px";
  newPoint.style.top = pointY+"px";
  if (start){
    newPoint.setAttribute("class", "pointStart");
    newPoint.style.cursor = "pointer";
    //newPoint.onclick = polyFinish;
    //newPoint.addEventListener("click", polyFinish, false);
  }
  currentPoly.appendChild(newPoint);
}

function polyFinish(event, construct){
  dump("polyfinish called\n");
  var len = currentPoly.childNodes.length;
  if (len >=3){
    var polyLeft = 1000000;
    var polyTop = 1000000;
    var polyWidth = 0;
    var polyHeight = 0;
    for(p=0; p<len; p++){
      var curEl = currentPoly.childNodes[p];
      curEl.setAttribute("class", "point");
      curEl.style.cursor = "default";
      pointLeft = parseInt(curEl.style.left);
      pointTop = parseInt(curEl.style.top);
      polyLeft = Math.min(polyLeft, pointLeft);
      polyTop = Math.min(polyTop, pointTop);
      dump(polyLeft+"\n");
    }
    for(p=0; p<len; p++){
      var curEl = currentPoly.childNodes[p];
      curEl.style.left = (parseInt(curEl.style.left)-polyLeft)+"px";
      curEl.style.top = (parseInt(curEl.style.top)-polyTop)+"px";
    }
    for(p=0; p<len; p++){
      var curEl = currentPoly.childNodes[p];
      polyWidth = Math.max(polyWidth, (parseInt(curEl.style.left)+3));
      polyHeight = Math.max(polyHeight, (parseInt(curEl.style.top)+3));
    }
    if (parseInt(currentPoly.style.left) >= 0){
      polyLeft += parseInt(currentPoly.style.left);
      polyTop += parseInt(currentPoly.style.top);
    }
    currentPoly.style.left = polyLeft+"px";
    currentPoly.style.top = polyTop+"px";
    currentPoly.style.width = polyWidth+"px";
    currentPoly.style.height = polyHeight+"px";
    //currentPoly.childNodes[0].onclick = null;
    //currentPoly.onclick = null;
    currentPoly.style.cursor = "auto";
    //currentPoly.childNodes[0].removeEventListener("click", polyFinish, false);
    //currentPoly.removeEventListener("click", addPoint, true);
    if (!construct)
      hotSpotProps(currentPoly);
  }
  else
    deleteElement(currentPoly);
  if (event)
    event.preventBubble();

  currentPoly = null;
}

function deleteElement(el){
  if (el){
    if (el.length){
      var len = currentElement.length;
      for(i=0; i<len; i++)
        frameDoc.body.removeChild(currentElement[i]);
    }
    else
      frameDoc.body.removeChild(el);
  }
}

function selectAll(){
  objList = frameDoc.getElementsByName("hotspot");
  listLen = objList.length;
  var objCount = 0;
  for(a=0; a<listLen; a++){
     selectElement(objList[a], objCount);
     objCount++;
  }
}

function selectElement(el, add){
  if (add){
    if (currentElement[0].getAttribute("class") != "poly"){
      len = currentElement[0].childNodes.length;
      for(i=0; i<len; i++)
        currentElement[0].childNodes[i].style.visibility = "hidden";
    }
    currentElement.push(el);
    document.getElementById("Map:Cut").setAttribute("disabled", "false");
    document.getElementById("Map:Copy").setAttribute("disabled", "false");
    return currentElement[currentElement.length-1];
  }
  else{
    if (currentElement[0]){
      if (currentElement[0].getAttribute("class") != "poly"){
        len = currentElement[0].childNodes.length;
        for(i=0; i<len; i++)
          currentElement[0].childNodes[i].style.visibility = "hidden";
      }
    }
    currentElement = null;
    currentElement = new Array();
    currentElement[0] = el;
    if (el != null){
    if (currentElement[0].getAttribute("class") != "poly"){
      len = currentElement[0].childNodes.length;
      for(i=0; i<len; i++)
        currentElement[0].childNodes[i].style.visibility = "visible";
    }
      document.getElementById("Map:Cut").setAttribute("disabled", "false");
      document.getElementById("Map:Copy").setAttribute("disabled", "false");
    return currentElement[0];
  }
    else{
      document.getElementById("Map:Cut").setAttribute("disabled", "true");
      document.getElementById("Map:Copy").setAttribute("disabled", "true");
    }
  }
}

function deSelectElement(el){
  var len = currentElement.length;
  var j=0;
  for(i=0; i<len; i++){
    dump(j+"\n");
    currentElement[j] = currentElement[i];
    if (currentElement[i] != el)
      j++;
  }
  currentElement.pop();
  if (currentElement.length == 1){
    selectElement(currentElement[0]);
  }
  if (currentElement.length >= 1){
    document.getElementById("Map:Cut").setAttribute("disabled", "false");
    document.getElementById("Map:Copy").setAttribute("disabled", "false");
  }
  else{
    document.getElementById("Map:Cut").setAttribute("disabled", "true");
    document.getElementById("Map:Copy").setAttribute("disabled", "true");
  }
}

function marqueeSelect(){
  marTop = parseInt(marquee.style.top);
  marLeft = parseInt(marquee.style.left);
  marRight = parseInt(marquee.style.width)+marLeft;
  marBottom = parseInt(marquee.style.height)+marTop;
  marquee.style.visibility = "hidden";
  marquee.style.top = "-5px";
  marquee.style.left = "-5px";
  marquee.style.width = "1px";
  marquee.style.height = "1px";
  marquee = null;
  objList = frameDoc.getElementsByName("hotspot");
  listLen = objList.length;
  var objCount = 0;
  for(a=0; a<listLen; a++){
    objTop = parseInt(objList[a].style.top);
    objLeft = parseInt(objList[a].style.left);
    objRight = parseInt(objList[a].style.width)+objLeft;
    objBottom = parseInt(objList[a].style.height)+objTop;
    if ((objTop >= marTop) && (objLeft >= marLeft) && (objBottom <= marBottom) && (objRight <= marRight)){
       //objList[i].style.borderColor = "#ffff00";
       selectElement(objList[a], objCount);
       objCount++;
    }
  }
}

function upMouse(event){
  if (currentTool != "poly"){
    if (marquee){
      marqueeSelect();
    }
    if (currentRect){
      if (!resize)
        hotSpotProps(currentRect);
      else
         resize = false;
    }
    else if (currentCir){
      if (!resize)
        hotSpotProps(currentCir);
      else
        resize = false;
    }
    else if (currentPoint)
      polyFinish(null, true);
      
    currentRect = null;
    currentCir = null;
    currentPoint=null;
    downTool = false;
    dragActive = false;
    dragObject = false;
    xlock = false;
    ylock = false;
  }
}

function moveMouse(event){
  if (downTool){
    endX = event.clientX+window.frames[0].pageXOffset;
    endY = event.clientY+window.frames[0].pageYOffset;

    if (dragActive){
      if (currentElement.length > 0){
        if (currentCir){
          radiusWidth = Math.abs((endX-startX));
          radiusHeight = Math.abs((endY-startY));
          circleRadius = Math.max(radiusWidth, radiusHeight);
          currentCir.style.top = Math.max(startY-circleRadius, 0)+"px";
          currentCir.style.left = Math.max(startX-circleRadius, 0)+"px";
          currentCir.style.width = (circleRadius*2)+"px";
          currentCir.style.height = (circleRadius*2)+"px";
        }
        else if (currentRect || marquee){
          var rectObject = (currentRect)? currentRect : marquee;
          if (!xlock){
            if (endX > startX){
              rectWidth = endX-startX;
              rectObject.style.left = Math.max(startX, 0 )+"px";
              rectObject.style.width = rectWidth+"px";
            }
            else{
              rectWidth = startX-endX;
              rectObject.style.left = Math.max(endX, 0)+"px";
              rectObject.style.width = rectWidth+"px";
            }
          }
          if (!ylock){
            if (endY > startY){
              rectHeight = endY-startY;
              rectObject.style.top = startY+"px";
              rectObject.style.height = rectHeight+"px";
            }
            else{
              rectHeight = startY-endY;
              rectObject.style.top = endY+"px";
              rectObject.style.height = rectHeight+"px";
            }
          }
        }
      }
    }
    else{
      if (currentTool == "rect"){
        if ((((endX-startX) > 1) || ((endX-startX) < -1)) && (((endY-startY) > 1) || ((endY-startY) < -1))){
          Rect();
          dragActive = true;
        }
      }
      if (currentTool == "cir"){
        if ((((endX-startX) > 1) || ((endX-startX) < -1)) && (((endY-startY) > 1) || ((endY-startY) < -1))){
          Circle();
          dragActive = true;
        }
      }
      if (currentTool == "pointer"){
        if (dragObject){
          var len = currentElement.length;
          var maxX = false;
          var maxY = false;
          for(i=0; i<len; i++){
            newX = Math.max(0, (endX-currentElement[i].startX));
            newY = Math.max(0, (endY-currentElement[i].startY));
            if (newX == 0)
              maxX = true;
            if (newY == 0)
              maxY = true;
          }
          for(i=0; i<len; i++){
            newX = Math.max(0, (endX-currentElement[i].startX));
            newY = Math.max(0, (endY-currentElement[i].startY));
            if ((newX > 0) && (maxX != true))
              currentElement[i].style.left = newX+"px";
            if ((newY >0) && (maxY != true))
              currentElement[i].style.top = newY+"px";
          }
        }
        else if (currentPoint){
           endX = endX-parseInt(currentPoint.parentNode.style.left);
           endY = endY-parseInt(currentPoint.parentNode.style.top);
           newX = Math.max((0-parseInt(currentPoint.parentNode.style.left)), (endX-currentPoint.startX));
           newY = Math.max((0-parseInt(currentPoint.parentNode.style.top)), (endY-currentPoint.startY));
           currentPoint.style.left = newX+"px";
           currentPoint.style.top = newY+"px";
        }
        else{
          marquee = frameDoc.getElementById("marquee");
          marquee.style.visibility = "visible";
          dragActive = true;
        }
      }
    }
  }
}


function downMouse(event){
  dump(event.target.parentNode.id+"\n");
  if (event.button == 0){
    if (currentTool != "poly"){
      startX = event.clientX+window.frames[0].pageXOffset;
      startY = event.clientY+window.frames[0].pageYOffset;
      downTool = true;
      if (currentTool == "pointer"){
        if (event.target.getAttribute("name") == "hotspot"){
          var el = event.target;
          var isSelected = false;

          if (event.target.getAttribute("cir") == "true")
            el = event.target.parentNode;

          if (event.shiftKey){
            var len = currentElement.length;
            var deselect = false;
            for(i=0; i<len; i++){
              if (currentElement[i] == el){
                deSelectElement(el);
                return;
              }
            }
            selectElement(el, true);
            isSelected = true; 
          }
          else{
            var len = currentElement.length;
            for(i=0; i<len; i++){
              if (currentElement[i] == el)
                isSelected = true;
            }
          }

          if (isSelected){
            var len = currentElement.length;
            for(i=0; i<len; i++){
              currentElement[i].startX = parseInt(event.clientX+window.frames[0].pageXOffset)-parseInt(currentElement[i].style.left);
              currentElement[i].startY = parseInt(event.clientY+window.frames[0].pageYOffset)-parseInt(currentElement[i].style.top);
            }
          }
          else{
            curObj = selectElement(el);
            curObj.startX = parseInt(event.clientX+window.frames[0].pageXOffset)-parseInt(curObj.style.left);
            curObj.startY = parseInt(event.clientY+window.frames[0].pageYOffset)-parseInt(curObj.style.top);
          }
          dragObject = true;
        }
        else if (event.target.getAttribute("name") == "handle"){
          dump("down on a handle\n");
          resize = true;
          el = event.target;
          curObj = selectElement(el.parentNode);
          if (curObj.className == "rect"){
            currentRect = curObj;
            switch (el.className){
              case "handletl":
                startX = parseInt(curObj.style.left)+parseInt(curObj.style.width);
                startY = parseInt(curObj.style.top)+parseInt(curObj.style.height);
                break;
              case "handletr":
                startX = parseInt(curObj.style.left);
                startY = parseInt(curObj.style.top)+parseInt(curObj.style.height);
                break;
              case "handlebl":
                startX = parseInt(curObj.style.left)+parseInt(curObj.style.width);
                startY = parseInt(curObj.style.top);
                break;
              case "handlebr":
                startX = parseInt(curObj.style.left);
                startY = parseInt(curObj.style.top);
                break;
              case "handlet":
                xlock = true;
                startY = parseInt(curObj.style.top)+parseInt(curObj.style.height);
                break;
              case "handleb":
                xlock = true;
                startY = parseInt(curObj.style.top);
                break;
              case "handlel":
                ylock = true;
                startX = parseInt(curObj.style.left)+parseInt(curObj.style.width);
                break;
              case "handler":
                ylock = true;
                startX = parseInt(curObj.style.left);
                break;
              deafult:
                return;
            } 
          }
          else{
            currentCir = curObj;
            startX = parseInt(curObj.style.left)+(parseInt(curObj.style.width)/2);
            startY = parseInt(curObj.style.top)+(parseInt(curObj.style.height)/2);
          }
        }
        else if (event.target.getAttribute("class") == "point"){
          dump("down on a point\n");
          selectElement(event.target.parentNode);
          currentPoint = event.target;
          currentPoint.startX = parseInt(event.clientX+window.frames[0].pageXOffset)-(parseInt(currentPoint.style.left)+parseInt(currentPoint.parentNode.style.left));
          currentPoint.startY = parseInt(event.clientY+window.frames[0].pageYOffset)-(parseInt(currentPoint.style.top)+parseInt(currentPoint.parentNode.style.top));
          currentPoly = currentPoint.parentNode;
        }
        else{
          dump(event.target+"\n");
          selectElement(null);
        }
      }
    }
  }
}

function clickMouse(event){
  if (event.button == 0){
    dump("body clicked\n");
    //alert(frameDoc.+'\n');
    startX = event.clientX+window.frames[0].pageXOffset;
    startY = event.clientY+window.frames[0].pageYOffset;
    if (currentTool == "poly"){
      if (event.target != currentPoly){
        if (currentPoly != null){
          if (event.target == currentPoly.childNodes[0]){
            polyFinish();
          }
          else if (event.detail == 2){
            polyFinish();
          }
        }
        else{
          Poly();
        }
      }
      else{
        addPoint(event);          
      }
    }
  }
}

function changeTool(event, what){
  if (!currentPoly){
    for(i=0; i<4; i++){
      buttonArray[i].setAttribute("toggled", 0);
      if (event.target == buttonArray[i]){
        buttonArray[i].setAttribute("toggled", 1);
      }
    }
    currentTool = what;
    if (currentTool != "pointer"){
      frameDoc.getElementById("bgDiv").style.cursor = "crosshair";
      frameDoc.body.style.cursor = "crosshair";
    }
    else{
      frameDoc.getElementById("bgDiv").style.cursor = "default";
      frameDoc.body.style.cursor = "default";
    }

    dump(what+" selected\n");
  }
  else {
    for(i=0; i<4; i++){
      if (event.target == buttonArray[i]){
        buttonArray[i].setAttribute("toggled", 0);
      }
    }
  }
}

function zoom(direction, ratio){
  dump('zoom called; ratio='+ratio+'\n');
  if (direction == "in")
    ratio = currentZoom*2;
  else if (direction == "out")
    ratio = currentZoom/2;

  if (ratio > 4 || ratio < 1 || ratio == currentZoom)
    return;

  if (ratio == 1){
    document.getElementById('Map:ZoomIn').setAttribute('disabled', 'false');
    document.getElementById('Map:ZoomOut').setAttribute('disabled', 'true');
    document.getElementById('Map:Apercent').setAttribute('checked', 'true');
    document.getElementById('Map:Bpercent').setAttribute('checked', 'false');
    document.getElementById('Map:Cpercent').setAttribute('checked', 'false');
  }
  else if (ratio == 4){
    document.getElementById('Map:ZoomIn').setAttribute('disabled', 'true');
    document.getElementById('Map:ZoomOut').setAttribute('disabled', 'false');
    document.getElementById('Map:Apercent').setAttribute('checked', 'false');
    document.getElementById('Map:Bpercent').setAttribute('checked', 'false');
    document.getElementById('Map:Cpercent').setAttribute('checked', 'true');
  }
  else {
    document.getElementById('Map:ZoomIn').setAttribute('disabled', 'false');
    document.getElementById('Map:ZoomOut').setAttribute('disabled', 'false');
    document.getElementById('Map:Apercent').setAttribute('checked', 'false');
    document.getElementById('Map:Bpercent').setAttribute('checked', 'true');
    document.getElementById('Map:Cpercent').setAttribute('checked', 'false');
  }

  objList = frameDoc.getElementsByName("hotspot");
  len = objList.length;
  for(i=0; i<len; i++){
    if (ratio > currentZoom){
      objList[i].style.width = (parseInt(objList[i].style.width)*(ratio/currentZoom))+"px";
      objList[i].style.height = (parseInt(objList[i].style.height)*(ratio/currentZoom))+"px";
      objList[i].style.top = (parseInt(objList[i].style.top)*(ratio/currentZoom))+"px";
      objList[i].style.left = (parseInt(objList[i].style.left)*(ratio/currentZoom))+"px";
    }
    else{
      objList[i].style.width = (parseInt(objList[i].style.width)/(currentZoom/ratio))+"px";
      objList[i].style.height = (parseInt(objList[i].style.height)/(currentZoom/ratio))+"px";
      objList[i].style.top = (parseInt(objList[i].style.top)/(currentZoom/ratio))+"px";
      objList[i].style.left = (parseInt(objList[i].style.left)/(currentZoom/ratio))+"px";
    }
    if (objList[i].getAttribute("class") == "poly"){
      pointList = objList[i].childNodes;
      plen = pointList.length;
      dump('i='+i+'\n');
      for(j=0; j<plen; j++){
        dump('i='+i+'\n');
        if (ratio > currentZoom){
          pointList[j].style.top = (parseInt(pointList[j].style.top)*(ratio/currentZoom))+"px";
          pointList[j].style.left = (parseInt(pointList[j].style.left)*(ratio/currentZoom))+"px";
        }
        else{
          pointList[j].style.top = (parseInt(pointList[j].style.top)/(currentZoom/ratio))+"px";
          pointList[j].style.left = (parseInt(pointList[j].style.left)/(currentZoom/ratio))+"px";
        }
      }
      currentPoly = objList[i];
      polyFinish(null, true);
      currentPoly = null;
    }
    dump('i='+i+'\n');
  }
  
  imgEl = frameDoc.getElementById("mainImg");
  bgDiv = frameDoc.getElementById("bgDiv");
  dump(imgEl.getAttribute("width")+'\n');
  if (ratio > currentZoom){
    imgEl.setAttribute("width", (parseInt(imgEl.offsetWidth)*(ratio/currentZoom)));
    imgEl.setAttribute("height", (parseInt(imgEl.offsetHeight)*(ratio/currentZoom)));
    bgDiv.style.width = imgEl.offsetWidth;
    bgDiv.style.height = imgEl.offsetHeight;
  }
  else{
    imgEl.setAttribute("width", (parseInt(imgEl.offsetWidth)/(currentZoom/ratio)));
    imgEl.setAttribute("height", (parseInt(imgEl.offsetHeight)/(currentZoom/ratio)));
    bgDiv.style.width = imgEl.offsetWidth;
    bgDiv.style.height = imgEl.offsetHeight;
  }
  currentZoom = ratio;  
}

function cutCopy(cut){
  len = currentElement.length;
  if (len >= 1){
    clipBoard = new Array();
    for (i=0; i<len; i++){
      el = currentElement[i];
      if (el.className == 'rect'){
        coords = parseInt(el.style.left)+","+parseInt(el.style.top)+","+(parseInt(el.style.left)+parseInt(el.style.width))+","+(parseInt(el.style.top)+parseInt(el.style.height));
        href = el.getAttribute('hsHref');
        target = el.getAttribute('hsTarget');
        alt = el.getAttribute('hsAlt');
        clipBoard[i] = 'Rect(\"'+coords+'\", \"'+href+'\", \"'+target+'\", \"'+alt+'\", true)';
      }
      else if (el.className == 'cir'){
        radius = Math.floor(parseInt(el.style.width)/2);
        coords = (parseInt(el.style.left)+radius)+","+(parseInt(el.style.top)+radius)+","+radius;
        href = el.getAttribute('href');
        target = el.getAttribute('hsTarget');
        alt = el.getAttribute('hsAlt');
        clipBoard[i] = 'Circle(\"'+coords+'\", \"'+href+'\", \"'+target+'\", \"'+alt+'\", true)';
      }
      else{
        var coords = '';
        var pointlen = el.childNodes.length;
        for(j=0; j<pointlen; j++){
          coords += (parseInt(el.style.left)+parseInt(el.childNodes[j].style.left))+","+(parseInt(el.style.top)+parseInt(el.childNodes[j].style.top))+",";
        }
        coords = coords.substring(0, (coords.length-1));
        href = el.getAttribute('href');
        target = el.getAttribute('hsTarget');
        alt = el.getAttribute('hsAlt');
        clipBoard[i] = 'Poly(\"'+coords+'\", \"'+href+'\", \"'+target+'\", \"'+alt+'\", true)';
      }
      if (cut){
        deleteElement(el);
      }
    }
    document.getElementById('Map:Paste').setAttribute('disabled', 'false');
  }
}

function paste(){
  len = clipBoard.length;
  func = '';
  for (i=0; i<len; i++){
    func += clipBoard[i]+'\;';
  }
  eval(func);
}

  function nudge(event, dir){
  /*prevent scrolling
  event.stopPropagation(); 
  event.preventDefault();*/

  len = currentElement.length;
  amount = 1;
  if (event.shiftKey)
    amount = 5;

  
  boundRectTop = 1000000;
  boundRectLeft = 1000000;
  for (i=0; i<len; i++){
    curTop = parseInt(currentElement[i].style.top);
    curLeft = parseInt(currentElement[i].style.left);
    if (curTop < boundRectTop)
      boundRectTop = curTop;
    if (curLeft < boundRectLeft)
      boundRectLeft = curLeft;
  }

  for (i=0; i<len; i++){
    if (dir == "up"){
      curTop = parseInt(currentElement[i].style.top);
      if (boundRectTop >= amount)
        currentElement[i].style.top = (curTop-amount) + "px";
      else
        currentElement[i].style.top = (curTop-boundRectTop) + "px";
    }
    else if (dir == "left"){
      curLeft = parseInt(currentElement[i].style.left);
      if (boundRectLeft >= amount)
        currentElement[i].style.left = (curLeft-amount) + "px";
      else
        currentElement[i].style.left = (curLeft-boundRectLeft) + "px";
    }
    else if (dir == "down"){
      curTop = parseInt(currentElement[i].style.top);
      currentElement[i].style.top = (curTop+amount) + "px";
    }
    else if (dir == "right"){
      curLeft = parseInt(currentElement[i].style.left);
      currentElement[i].style.left = (curLeft+amount) + "px";
    }
  }
}
  