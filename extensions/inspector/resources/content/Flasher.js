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

function Flasher(aShell, aColor, aThickness, aDuration, aSpeed)
{
  this.mShell = aShell;
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
  mWindow:null,
  mRegistryId: null,
  mFlashes: 0,
  mStartTime: 0,
  mColor: null,
  mThickness: 0,
  mDuration: 0,
  mSpeed: 0,

  ////////////////////////////////////////////////////////////////////////////
  //// Properties

  get flashing() { return this.mFlashTimeout; },
  
  get element() { return this.mElement; },
  set element(val) 
  { 
    if (val && val.nodeType == 1) 
      this.mElement = val; 
    else 
      throw "Invalid node type.";
  },

  get window() { return this.mWindow; },
  set window(aVal) { this.mWindow = aVal; },

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

  start: function()
  {
    this.mFlashes = 0;
    this.mStartTime = new Date();
    this.doFlash();
  },

  doFlash: function()
  {
    if (this.mFlashes%2) {
      this.paintOn();
    } else {
      this.paintOff();
    }
    this.mFlashes++;

    if (new Date() - this.mStartTime < this.mDuration) {
      this.mFlashTimeout = window.setTimeout("gFlasherRegistry["+this.mRegistryId+"].doFlash()", this.mSpeed);
    } else {
      this.stop();
    }
},

  stop: function()
  {
    window.clearTimeout(this.mFlashTimeout);
    this.mFlashTimeout = null;
    this.paintOff();
  },

  paintOn: function()
  {
    this.mShell.drawElementOutline(this.mElement, this.mWindow, this.mColor, this.mThickness);

  },

  paintOff: function()
  {
    this.mShell.repaintElement(this.mElement, this.mWindow);
  }

};

