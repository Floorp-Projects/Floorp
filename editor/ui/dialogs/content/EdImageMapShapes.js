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





var downTool = false;

var dragActive = false;

var dragObject = false;

var startX = null;

var startY = null;

var endX = null;

var endY = null;

var downTool = false;

var currentElement = new Array();

var currentTool = "rect";

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

var marquee = null;

var frameDoc = null;

var buttonArray = new Array();

var resize = false;



function Rect(coords, href, target, title, construct){

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

    currentRect.width = endX+"px";

    currentRect.height = endX+"px";

  }

  else{

    var coordArray = coords.split(',');

    currentRect.style.left = coordArray[0]+"px";

    currentRect.style.top = coordArray[1]+"px";

    currentRect.style.width = (parseInt(coordArray[2])-parseInt(coordArray[0]))+"px";

    currentRect.style.height = (parseInt(coordArray[3])-parseInt(coordArray[1]))+"px";

    if (href)

      currentRect.setAttribute("href", href);

    if (target)

      currentRect.setAttribute("target", target);

    if (title)

      currentRect.setAttribute("title", title);

  }

  if (construct)

    currentRect = null;

}



function Circle(coords, href, target, title, construct){

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

    currentCir.width = endX+"px";

    currentCir.height = endX+"px";

  }

  else{

    var coordArray = coords.split(',');

    radius = parseInt(coordArray[2]);

    currentCir.style.left = (parseInt(coordArray[0])-radius)+"px";

    currentCir.style.top = (parseInt(coordArray[1])-radius)+"px";

    currentCir.style.width = (radius*2)+"px";

    currentCir.style.height = (radius*2)+"px";

    if (href)

      currentCir.setAttribute("href", href);

    if (target)

      currentCir.setAttribute("target", target);

    if (title)

      currentCir.setAttribute("title", title);

  }

  if (construct)

    currentCir = null;



  /*cirImg = frameDoc.createElement("img");

  cirImg.setAttribute("src", "circleobject.gif");

  cirImg.setAttribute("name", "hotspot");

  cirImg.setAttribute("cir", "true");

  cirImg.style.width = "100%";

  cirImg.style.height = "100%";

  currentCir.appendChild(cirImg);*/

}



function Poly(coords, href, target, title, construct){

  newPoly = frameDoc.createElement("div");

  newPoly.setAttribute("class", "poly");

  newPoly.setAttribute("id", "poly"+polyCount++);

  newPoly.setAttribute("name", "hotspot");

  currentPoly = selectElement(frameDoc.body.appendChild(newPoly));

  if (!coords){

    addPoint(startX, startY, true);

  }

  else{

    var coordArray = coords.split(',');

    var len = coordArray.length;

    for (i=0; i<len; i++){

      addPoint(coordArray[i], coordArray[i+1]);

      i++;

    }

    if (href)

      currentPoly.setAttribute("href", href);

    if (target)

      currentPoly.setAttribute("target", target);

    if (title)

      currentPoly.setAttribute("title", title);

    polyFinish(null, construct);

  }

}



function addPoint(pointX, pointY, start){

  newPoint = frameDoc.createElement("div");

  newPoint.setAttribute("class", "point");

  newPoint.setAttribute("id", "point"+pointCount++);

  newPoint.style.left = pointX+"px";

  newPoint.style.top = pointY+"px";

  if (start){

    newPoint.setAttribute("class", "pointStart");

    newPoint.style.cursor = "sw-resize";

  }

  currentPoly.appendChild(newPoint);

}



function polyFinish(event, construct){

  var len = currentPoly.childNodes.length;

  if (len >=3){

    var polyLeft = 1000000;

    var polyTop = 1000000;

    var polyWidth = 0;

    var polyHeight = 0;

    for(i=0; i<len; i++){

      var curEl = currentPoly.childNodes[i];

      curEl.setAttribute("class", "point");

      curEl.style.cursor = "default";

      pointLeft = parseInt(curEl.style.left);

      pointTop = parseInt(curEl.style.top);

      polyLeft = Math.min(polyLeft, pointLeft);

      polyTop = Math.min(polyTop, pointTop);

      dump(polyLeft+"\n");

    }

    for(i=0; i<len; i++){

      var curEl = currentPoly.childNodes[i];

      curEl.style.left = (parseInt(curEl.style.left)-polyLeft)+"px";

      curEl.style.top = (parseInt(curEl.style.top)-polyTop)+"px";

    }

    for(i=0; i<len; i++){

      var curEl = currentPoly.childNodes[i];

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

    if (!construct)

      hotSpotProps(currentPoly);

    currentPoly = null;

  }

  else

    deleteElement(currentPoly);

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



function selectElement(el, add){

  if (add){

    currentElement.push(el);

    return currentElement[currentElement.length-1];

  }

  else{

    currentElement = null;

    currentElement = new Array();

    currentElement[0] = el;

    return currentElement[0];

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

  dump(currentElement.length+"\n");

}



function marqueeSelect(){

  marTop = parseInt(marquee.style.top);

  marLeft = parseInt(marquee.style.left);

  marRight = parseInt(marquee.style.width)+marLeft;

  marBottom = parseInt(marquee.style.height)+marTop;

  marquee.style.visibility = "hidden";

  marquee.style.top = "1px";

  marquee.style.left = "1px";

  marquee.style.width = "1px";

  marquee.style.height = "1px";

  marquee = null;

  objList = frameDoc.getElementsByName("hotspot");

  len = objList.length;

  var objCount = 0;

  for(i=0; i<len; i++){

    objTop = parseInt(objList[i].style.top);

    objLeft = parseInt(objList[i].style.left);

    objRight = parseInt(objList[i].style.width)+objLeft;

    objBottom = parseInt(objList[i].style.height)+objTop;

    if ((objTop >= marTop) && (objLeft >= marLeft) && (objBottom <= marBottom) && (objRight <= marRight)){

       //objList[i].style.borderColor = "#ffff00";

       selectElement(objList[i], objCount);

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

    endX = event.clientX;

    endY = event.clientY;



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

      if (currentTool == "select"){

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

          marquee.style.visibility = "inherit";

          dragActive = true;

        }

      }

    }

  }

}





function downMouse(event){

  dump(event.target.parentNode.id+"\n");

  if (event.button == 1){

    if (currentTool != "poly"){

      startX = event.clientX;

      startY = event.clientY;

      downTool = true;

      if (currentTool == "select"){

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

              currentElement[i].startX = parseInt(event.clientX)-parseInt(currentElement[i].style.left);

              currentElement[i].startY = parseInt(event.clientY)-parseInt(currentElement[i].style.top);

            }

          }

          else{

            curObj = selectElement(el);

            curObj.startX = parseInt(event.clientX)-parseInt(curObj.style.left);

            curObj.startY = parseInt(event.clientY)-parseInt(curObj.style.top);

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

          currentPoint = event.target;

          currentPoint.startX = parseInt(event.clientX)-(parseInt(currentPoint.style.left)+parseInt(currentPoint.parentNode.style.left));

          currentPoint.startY = parseInt(event.clientY)-(parseInt(currentPoint.style.top)+parseInt(currentPoint.parentNode.style.top));

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

  if (event.button == 1 && event.target.tagName != "INPUT"){

    dump("body clicked\n");

    startX = event.clientX;

    startY = event.clientY;

    if (currentTool == "poly"){

      dump(event.clickCount+"\n");

      if (event.target == currentPoly)

        addPoint(startX, startY);

      else

        Poly();

    }

  }

}



function changeTool(event, what){

  if (!currentPoly){

    for(i=0; i<4; i++){

      buttonArray[i].setAttribute("toggled", 0);

    }

    currentTool = what;

    dump(what+" selected\n");

  }

}