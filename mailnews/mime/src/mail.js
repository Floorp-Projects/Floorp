/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
var _dQueue = null;
var _interval = null;
var _wPrev = 0;
var _wH = 0;
var isNav4, isIE4;

if (parseInt(navigator.appVersion.charAt(0)) >= 4) 
{
  isNav4 = (navigator.appName == "Netscape") ? true : false;
  isIE4 = (navigator.appName.indexOf("Microsoft") != -1) ? true : false;
}

function getTop(layer) 
{ 
  if (isNav4) 
    return layer.top; 
  return layer.style.pixelTop; 
}

function Drift() 
{
  this.min = false; this.max = false;
  this.tOffset = -.069; this.bOffset = 0;
  this.lOffset = 0; this.interval = 200;
  this.startDrift = startDrift;
  return this;
}

var prefs;
function startDrift(layer) 
{
  if(_wH == 0) 
  { 
    if(isNav4) 
        _wH = window.innerHeight; 
    else 
        _wH = document.body.clientHeight; 
  }

  layer.tOffset = getTop(layer);
  if(!this.interval) 
  {
    prefs = new Drift();
    prefs.temp = true;
  } 
  else 
    prefs = this;

  layer.min = prefs.min;
  layer.max = prefs.max;
  if(prefs.tOffset != -.069) 
    layer.tOffset = prefs.tOffset; // offset from top of window

  layer.bOffset = prefs.bOffset; // offset from bottom of window
  layer.lOffset = prefs.lOffset; // for nested layers

  if(_dQueue == null) 
    _dQueue = new Array();

  _dQueue[_dQueue.length] = layer;
  if (_interval == null) 
    _interval = setInterval("checkDrift()", prefs.interval);
  if (prefs.temp) 
    prefs = null;
  return (_dQueue.length-1);
}

var str, i, top, bottom;
function getClipHeight(layer) 
{
  if(isNav4) 
    return layer.clip.height;

  str = layer.style.clip;
  if(str) 
  {
    i = str.indexOf("(");
    top = parseInt(str.substring(i + 1, str.length));
    i = str.indexOf(" ", i + 1); i = str.indexOf(" ", i + 1);
    bottom = parseInt(str.substring(i + 1, str.length));
    if(top < 0) 
      top = 0;
    return (bottom-top);
  }

  return layer.style.pixelHeight;
}

var IMGW= 117;
var IMGH= 18;
var LSAFETY= 40;
var TSAFETY= 0;

var layer, dst, wOff;
function checkDrift() 
{
alert("here\n");
  for(i = 0; i < _dQueue.length; i++) 
  {
    layer = _dQueue[i];
    if (!layer) continue;

    IMGH = layer.style.height;
    IH= document.body.clientHeight;
    IW= document.body.clientWidth;
    PX= document.body.scrollLeft;
    PY= document.body.scrollTop;
/*    layer.style.pixelTop = (IH+PY-(IMGH+TSAFETY)); 
    brand.style.left=(IW+PX-(IMGW+LSAFETY));
*/

    layer.style.top = (IH+PY-(IMGH+TSAFETY)); 
  }
return;

  if(isNav4) 
    wOff = window.pageYOffset;
  else 
    wOff = document.body.scrollTop;

  for(i = 0; i < _dQueue.length; i++) 
  {
    layer = _dQueue[i];
    if (!layer) continue;
    if(wOff > _wPrev) 
    {
      if (getClipHeight(layer)+layer.bOffset+layer.tOffset > _wH) 
      {
	      if(wOff+_wH-layer.bOffset > getTop(layer)+layer.lOffset+getClipHeight(layer))
      	  dst = wOff+_wH-layer.lOffset-getClipHeight(layer)-layer.bOffset;
      } 
      else 
        dst = wOff-layer.lOffset+layer.tOffset;
    } 
    else if (wOff < _wPrev) 
    {
      if(getClipHeight(layer)+layer.bOffset+layer.tOffset <= _wH)
	      dst = wOff-layer.lOffset+layer.tOffset;
      else if(wOff < getTop(layer)+layer.lOffset-layer.tOffset)
	      dst = wOff-layer.lOffset+layer.tOffset;
    } 

    if (layer.max) 
      if (dst > layer.max) 
        dst = layer.max;

    if (layer.min || layer.min == 0)
      if(dst < layer.min) 
        dst = layer.min; 

    if(dst || dst == 0) 
    {
      if (isNav4) 
        layer.top = dst;
      else 
        layer.style.pixelTop = dst;
    }
  }

  _wPrev = wOff;
}

function stopDrift(id) 
{ 
  _dQueue[id] = null; 
}


function toggleAttachments() 
{ 
  alert("Cool Stuff!");
  return;
  
  var target = document.getElementById("attachPane");

    IMGH = target.style.height;
    IH= document.body.clientHeight;
    IW= document.body.clientWidth;
    PX= document.body.scrollLeft;
    PY= document.body.scrollTop;
    target.style.top = (IH+PY-(IMGH+TSAFETY)); 
    alert("top = target.style.top\n");
return;

  if (target.style.display == "")
    target.style.display = "none";
  else
  {
    if (!drifting) startDrift(target); 
    target.style.display = "";  
    drifting = 1;
  }
}
