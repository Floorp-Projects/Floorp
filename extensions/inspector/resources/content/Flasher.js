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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* Flasher ---------------------------------------------------
*   Object for controlling a timed flashing animation which 
*   paints a border around an element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var gFlasherRegistry = [];

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class Flasher

function Flasher(aColor, aThickness, aDuration, aSpeed)
{
  this.mShell = XPCU.createInstance("@mozilla.org/inspector/flasher;1", "inIFlasher");;
  this.color = aColor;
  this.mThickness = aThickness;
  this.duration = aDuration;
  this.mSpeed = aSpeed;

  this.register();
}

Flasher.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  mFlashTimeout: null,
  mElement:null,
  mRegistryId: null,
  mFlashes: 0,
  mStartTime: 0,
  mColor: null,
  mThickness: 0,
  mDuration: 0,
  mSpeed: 0,

  ////////////////////////////////////////////////////////////////////////////
  //// Properties

  get flashing() { return this.mFlashTimeout != null; },
  
  get element() { return this.mElement; },
  set element(val) 
  { 
    if (val && val.nodeType == 1) {
      this.mElement = val; 
    } else 
      throw "Invalid node type.";
  },

  get color() { return this.mColor; },
  set color(aVal) { if (aVal.charAt(0) == '#') aVal = aVal.substr(1); this.mColor = aVal; },

  get thickness() { return this.mThickness; },
  set thickness(aVal) { this.mThickness = aVal; },

  get duration() { return this.mDuration; },
  set duration(aVal) { this.mDuration = aVal*1000; /*seconds->milliseconds*/ },

  get speed() { return this.mSpeed; },
  set speed(aVal) { this.mSpeed = aVal; },

  // :::::::::::::::::::::::::::::::::::::::::::::::::::::::::
  // :::::::::::::::::::: Methods ::::::::::::::::::::::::::::
  // :::::::::::::::::::::::::::::::::::::::::::::::::::::::::

  register: function()
  {
    var length = gFlasherRegistry.length;
    gFlasherRegistry[length] = this;
    this.mRegistryId = length;
  },

  start: function(aDuration, aSpeed, aHold)
  {
    this.mUDuration = aDuration ? aDuration*1000 : this.mDuration;
    this.mUSpeed = aSpeed ? aSpeed : this.mSpeed
    this.mHold = aHold;
    this.mFlashes = 0;
    this.mStartTime = new Date();
    this.doFlash();
  },

  doFlash: function()
  {
    if (this.mHold || this.mFlashes%2) {
      this.paintOn();
    } else {
      this.paintOff();
    }
    this.mFlashes++;

    if (this.mUDuration < 0 || new Date() - this.mStartTime < this.mUDuration) {
      this.mFlashTimeout = window.setTimeout("gFlasherRegistry["+this.mRegistryId+"].doFlash()", this.mUSpeed);
    } else {
      this.stop();
    }
},

  stop: function()
  {
    if (this.flashing) {
      window.clearTimeout(this.mFlashTimeout);
      this.mFlashTimeout = null;
      this.paintOff();
    }
  },

  paintOn: function()
  {
    this.mShell.drawElementOutline(this.mElement, this.mColor, this.mThickness);
  },

  paintOff: function()
  {
    this.mShell.repaintElement(this.mElement);
  }

};

