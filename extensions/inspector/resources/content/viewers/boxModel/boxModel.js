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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

/***************************************************************
* BoxModelViewer --------------------------------------------
*  The viewer for the boxModel and visual appearance of an element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

const kIMPORT_RULE = Components.interfaces.nsIDOMCSSRule.IMPORT_RULE;

//////////////////////////////////////////////////

window.addEventListener("load", BoxModelViewer_initialize, false);
window.addEventListener("load", BoxModelViewer_destroy, false);

function BoxModelViewer_initialize()
{
  viewer = new BoxModelViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

function BoxModelViewer_destroy()
{
  viewer.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class BoxModelViewer

function BoxModelViewer()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
}

BoxModelViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,
  mLastBitmapId: null,
  mColorPicker: null,
  mOutlinesReady: false,
  mOutlines: false,
  mFills: false,
  mZoomScale: 1,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "boxModel" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mSubject = aObject;
    
    if (this.mBitmap) {
      this.mBitmapDepot.remove(this.mLastBitmapId);
      this.mBitmap = null;
    }
    
    document.getElementById("bxZoomStack").setAttribute("hidden", "true");
      
    this.updateStatGroup();
    
    this.updateCommand("cmdZoom", "disabled", true);
    this.updateCommand("cmdToggleFill", "disabled", true);
    this.updateCommand("cmdToggleOutlines", "disabled", true);
    this.updateCommand("cmdOpenColorPicker", "disabled", true);
    this.updateCommand("cmdSaveImage", "disabled", true);
    
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    
    this.mImgZoom = document.getElementById("imgZoom");

    this.mScreenCap = XPCU.getService("@mozilla.org/inspector/screen-capturer;1", "inIScreenCapturer");
    if (this.mScreenCap)
      this.mBitmapDepot = XPCU.getService("@mozilla.org/inspector/bitmap-depot;1", "inIBitmapDepot");
    else {
      // screen capturer may not be implemented on all platforms, so hide the fancy stuff
      document.getElementById("bxCaptureStuff").setAttribute("hidden", "true");      
    }
    
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
  },

  updateCommand: function(aCmd, aAttr, aValue)
  {
    if (aValue == null)
      document.getElementById(aCmd).removeAttribute(aAttr);
    else
      document.getElementById(aCmd).setAttribute(aAttr, aValue);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// image capture

  capture: function()
  {
    // perform screen capture, cache image, and display image
    if (this.mScreenCap) {
      try { 
        var bitmap = this.mScreenCap.captureElement(this.mSubject);
      } catch (ex) {
        // failed to do screen capture - die silently
        return;
      }
      
      if (bitmap) {
        // create unique id for the bitmap and store it in bitmap depot
        var bitmapId = "inspector-box-object" + (new Date()).getTime();
        this.mBitmap = bitmap;
        this.mLastBitmapId = bitmapId;
        this.mBitmapDepot.put(bitmap, bitmapId);
        
        // display bitmap in document
        document.getElementById("bxZoomStack").removeAttribute("hidden");
        this.mImgZoom.setAttribute("src", "moz-bitmap:"+bitmapId);

        // zoom in to current zoom setting
        this.zoom(document.getElementById("mlZoom").value);
  
        // enable all commands sensitive to presence of bitmap
        this.updateCommand("cmdZoom", "disabled", null);
        this.updateCommand("cmdToggleFill", "disabled", null);
        this.updateCommand("cmdToggleOutlines", "disabled", null);
        this.updateCommand("cmdOpenColorPicker", "disabled", null);
        this.updateCommand("cmdSaveImage", "disabled", null);
      } 
    }
    
    
  },
  
  zoomMenulist: function()
  {
    var ml = document.getElementById("mlZoom");
    this.zoom(ml.value);
  },

  zoom: function(aScale)
  {
    var s = parseInt(aScale);
    this.mZoomScale = s;
    var w = this.mBitmap.width * s;
    var h = this.mBitmap.height * s;
    this.mImgZoom.setAttribute("style", "min-width: " + w + "px; min-height: " + h + "px;");
    
    this.mOutlinesReady = false;
    if (this.showOutlines || this.fillOutlines)
      this.calculateOutlines();
  },
  
  get showOutlines() 
  {
    return document.getElementById("cbxShowOutlines").getAttribute("checked") == "true";
  },
  
  get fillOutlines() 
  {
    return document.getElementById("cbxFillOutlines").getAttribute("checked") == "true";
  },
  
  getSubjectComputedStyle: function()
  {
    var view = this.mSubject.ownerDocument.defaultView;
    return view.getComputedStyle(this.mSubject, "");
  },
  
  calculateOutlines: function()
  {
    var computed = this.getSubjectComputedStyle();
    var s = this.mZoomScale;
    
    // fill out the margin outline
    var mt = parseInt(computed.getPropertyCSSValue("margin-top").cssText) * s;
    var mr = parseInt(computed.getPropertyCSSValue("margin-right").cssText) * s;
    var mb = parseInt(computed.getPropertyCSSValue("margin-bottom").cssText) * s;
    var ml = parseInt(computed.getPropertyCSSValue("margin-left").cssText) * s;
    var bx = document.getElementById("bxMargin");
    bx.setAttribute("style", (mt ? ("padding-top: " + mt + "px;") : "") +
                             (mr ? ("padding-right: " + mr + "px;") : "") +
                             (mb ? ("padding-bottom: " + mb + "px;") : "") +
                             (ml ? ("padding-left: " + ml + "px;") : ""));
    
    // fill out the border outline
    var bt = parseInt(computed.getPropertyCSSValue("border-top-width").cssText) * s;
    var br = parseInt(computed.getPropertyCSSValue("border-right-width").cssText) * s;
    var bb = parseInt(computed.getPropertyCSSValue("border-bottom-width").cssText) * s;
    var bl = parseInt(computed.getPropertyCSSValue("border-left-width").cssText) * s;
    bx = document.getElementById("bxBorder");
    bx.setAttribute("style", (bt ? ("padding-top: " + (bt-1) + "px;") : "") +
                             (br ? ("padding-right: " + (br-1) + "px;") : "") +
                             (bb ? ("padding-bottom: " + (bb-1) + "px;") : "") +
                             (bl ? ("padding-left: " + (bl-1) + "px;") : ""));
                             
    // fill out the padding outline
    var pt = parseInt(computed.getPropertyCSSValue("padding-top").cssText) * s;
    var pr = parseInt(computed.getPropertyCSSValue("padding-right").cssText) * s;
    var pb = parseInt(computed.getPropertyCSSValue("padding-bottom").cssText) * s;
    var pl = parseInt(computed.getPropertyCSSValue("padding-left").cssText) * s;
    var bx = document.getElementById("bxPadding");
    bx.setAttribute("style", (pt ? ("padding-top: " + (pt-2) + "px;") : "") +
                             (pr ? ("padding-right: " + (pr-2) + "px;") : "") +
                             (pb ? ("padding-bottom: " + (pb-2) + "px;") : "") +
                             (pl ? ("padding-left: " + (pl-2) + "px;") : ""));

    this.mOutlinesReady = true;
  },  
  
  toggleOutlines: function()
  {
    this.mOutlines = !this.mOutlines;
    document.getElementById("bxMargin").setAttribute("outline", this.mOutlines);
    document.getElementById("bxBorder").setAttribute("outline", this.mOutlines);
    document.getElementById("bxPadding").setAttribute("outline", this.mOutlines);
    document.getElementById("bxContent").setAttribute("outline", this.mOutlines);
    
    if (!this.mOutlinesReady)
      this.calculateOutlines();
  },
  
  toggleFill: function()
  {
    this.mFills = !this.mFills;
    document.getElementById("bxMargin").setAttribute("fill", this.mFills);
    document.getElementById("bxBorder").setAttribute("fill", this.mFills);
    document.getElementById("bxPadding").setAttribute("fill", this.mFills);
    document.getElementById("bxContent").setAttribute("fill", this.mFills);

    if (!this.mOutlinesReady)
      this.calculateOutlines();
  },
  
  
  ////////////////////////////////////////////////////////////////////////////
  //// statistical updates
  
  updateStatGroup: function()
  {
    var ml = document.getElementById("mlStats");
    this.showStatGroup(ml.value);
  },
  
  switchStatGroup: function(aGroup)
  {
    var ml = document.getElementById("mlStats");
    ml.value = aGroup;
    this.showStatGroup(aGroup);
  },
  
  showStatGroup: function(aGroup)
  {
    if (aGroup == "position") {
      this.showPositionStats();
    } else if (aGroup == "dimension") {
      this.showDimensionStats();
    } else if (aGroup == "margin") {
      this.showMarginStats();
    } else if (aGroup == "border") {
      this.showBorderStats();
    } else if (aGroup == "padding") {
      this.showPaddingStats();
    }    
  },
  
  showStatistic: function(aCol, aRow, aSide, aSize)
  {
    var label = document.getElementById("txR"+aRow+"C"+aCol+"Label");
    var val = document.getElementById("txR"+aRow+"C"+aCol+"Value");
    label.setAttribute("value", aSide && aSide.length ? aSide + ":" : "");
    val.setAttribute("value", aSize);
  },
  
  showPositionStats: function()
  {
    if ("boxObject" in this.mSubject) { // xul elements
      var bx = this.mSubject.boxObject;
      this.showStatistic(1, 1, "x", bx.x);
      this.showStatistic(1, 2, "y", bx.y);
      this.showStatistic(2, 1, "screen x", bx.screenX);
      this.showStatistic(2, 2, "screen y", bx.screenY);
    } else { // html elements
      this.showStatistic(1, 1, "x", this.mSubject.offsetLeft);
      this.showStatistic(1, 2, "y", this.mSubject.offsetTop);
      this.showStatistic(2, 1, "", "");
      this.showStatistic(2, 2, "", "");
    }
  },
  
  showDimensionStats: function()
  {
    if ("boxObject" in this.mSubject) { // xul elements
      var bx = this.mSubject.boxObject;
      this.showStatistic(1, 1, "box width", bx.width);
      this.showStatistic(1, 2, "box height", bx.height);
      this.showStatistic(2, 1, "content width", "");
      this.showStatistic(2, 2, "content height", "");
      this.showStatistic(3, 1, "", "");
      this.showStatistic(3, 2, "", "");
    } else { // html elements
      this.showStatistic(1, 1, "box width", this.mSubject.offsetWidth);
      this.showStatistic(1, 2, "box height", this.mSubject.offsetHeight);
      this.showStatistic(2, 1, "content width", "");
      this.showStatistic(2, 2, "content height", "");
      this.showStatistic(3, 1, "", "");
      this.showStatistic(3, 2, "", "");
    }
  },

  showMarginStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readMarginStyle(style, "top"), this.readMarginStyle(style, "right"), 
                this.readMarginStyle(style, "bottom"), this.readMarginStyle(style, "left")];
    this.showSideStats("margin", data);                
  },

  showBorderStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readBorderStyle(style, "top"), this.readBorderStyle(style, "right"), 
                this.readBorderStyle(style, "bottom"), this.readBorderStyle(style, "left")];
    this.showSideStats("border", data);                
  },

  showPaddingStats: function()
  {
    var style = this.getSubjectComputedStyle();
    var data = [this.readPaddingStyle(style, "top"), this.readPaddingStyle(style, "right"), 
                this.readPaddingStyle(style, "bottom"), this.readPaddingStyle(style, "left")];
    this.showSideStats("padding", data);
  },

  onRegionOver: function(aEvent)
  {
    if (aEvent.target == document.getElementById("bxMargin")) { // margin area
      this.switchStatGroup("margin");
    } else if (aEvent.target == document.getElementById("bxBorder")) { // border area
      this.switchStatGroup("border");
    } else if (aEvent.target == document.getElementById("bxPadding")) { // padding area
      this.switchStatGroup("padding");
    } else if (aEvent.target == document.getElementById("bxContent")) { // content area
      this.switchStatGroup("dimension");
    }
  },
  
  showSideStats: function(aName, aData)
  {
    this.showStatistic(1, 1, aName+"-top", aData[0]);
    this.showStatistic(2, 1, aName+"-right", aData[1]);
    this.showStatistic(1, 2, aName+"-bottom", aData[2]);
    this.showStatistic(2, 2, aName+"-left", aData[3]);
    this.showStatistic(3, 1, "", "");
    this.showStatistic(3, 2, "", "");
  },
  
  onRegionOut: function(aEvent)
  {
    this.switchStatGroup("position");
  },
  
  readMarginStyle: function(aStyle, aSide)
  {
    return aStyle.getPropertyCSSValue("margin-"+aSide).cssText;
  },
  
  readPaddingStyle: function(aStyle, aSide)
  {
    return aStyle.getPropertyCSSValue("padding-"+aSide).cssText;
  },
  
  readBorderStyle: function(aStyle, aSide)
  {
    var style = aStyle.getPropertyCSSValue("border-"+aSide+"-style").cssText;
    if (!style || !style.length) {
      return "none";
    } else {
      return aStyle.getPropertyCSSValue("border-"+aSide+"-width").cssText + " " + 
             style + " " +
             aStyle.getPropertyCSSValue("border-"+aSide+"-color").cssText;
    }
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// color picking
  
  openColorPicker: function()
  {
    if (this.mColorPicker) return;
    
    this.mColorPicker = 
      window.openDialog("chrome://inspector/content/viewers/boxModel/colorPicker.xul", 
                        "_blank", "chrome,dependent");

    var cmd = document.getElementById("cmdOpenColorPicker");
    cmd.setAttribute("disabled", "true");
  },
  
  onColorPickerClosed: function()
  {
    this.mColorPicker = null;

    var cmd = document.getElementById("cmdOpenColorPicker");
    cmd.removeAttribute("disabled");
  },
  
  startPickColor: function(aDialogWin)
  {
    this.mColorDialogWin = aDialogWin;
    this.mColorPicking = true;
    
    var box = document.getElementById("bxZoomStack");
    box.addEventListener("mousemove", ColorPickMouseMove, false);
    box.addEventListener("mousedown", ColorPickMouseDown, false);
  },
  
  stopPickColor: function()
  {
    if (!this.mColorPicking) return;
    this.mColorPicking = false;
    
    var box = document.getElementById("bxZoomStack");
    box.removeEventListener("mousemove", ColorPickMouseMove, false);
    box.removeEventListener("mousedown", ColorPickMouseDown, false);

    this.mColorDialogWin.onStopPicking();
  },
  
  captureColor: function(aEvent)
  {
    var bx = document.getElementById("bxMargin").boxObject;
    var x = aEvent.clientX - bx.x;
    var y = aEvent.clientY - bx.y;

    var c = this.mBitmap.getPixelHex(Math.floor(x/this.mZoomScale), Math.floor(y/this.mZoomScale));
    this.showColor(c);
  },
  
  showColor: function(aColor)
  {
    var doc = this.mColorDialogWin.document;
    var tx = doc.getElementById("txColor");
    tx.setAttribute("value", aColor);
    var tx = doc.getElementById("bxColorSwatch");
    tx.setAttribute("style", "background-color: " + aColor);
  },
  
  saveImage: function()
  {
    var encoder = XPCU.createInstance("@mozilla.org/inspector/png-encoder;1", "inIPNGEncoder");
    var path = FilePickerUtils.pickFile("Save Image as PNG", "", ["filterAll"], "Save");
    if (path)
     encoder.writePNG(this.mBitmap, path.unicodePath, 24);
  }

  ////////////////////////////////////////////////////////////////////////////
  //// image scrolling
  /*
  startScroll: function(aEvent)
  {
    var zoomBx = document.getElementById("sbxZoom");
    zoomBx.addEventListener("mousemove", ImageScrollMouseMove, false);
    zoomBx.addEventListener("mouseup", ImageScrollMouseUp, false);
    
    this.mScrollBaseX = aEvent.clientX;
    this.mScrollBaseY = aEvent.clientY;
  },
  
  stopScroll: function(aEvent)
  {
    var zoomBx = document.getElementById("sbxZoom");
    zoomBx.removeEventListener("mousemove", ImageScrollMouseMove, false);
    zoomBx.removeEventListener("mouseup", ImageScrollMouseUp, false);
  },
  
  doScroll: function(aEvent)
  {
    var zoomBx = document.getElementById("sbxZoom");
    var bx = zoomBx.boxObject.QueryInterface(Components.interfaces.nsIScrollBoxModel);
    
    var xd = aEvent.clientX - this.mScrollBaseX;
    var yd = aEvent.clientY - this.mScrollBaseY;
    this.mScrollBaseX += xd;
    this.mScrollBaseY += yd;
    
    var xdata = {};
    var ydata = {};
    bx.getPosition(xdata, ydata);
    
    // scrollBy is not implemented yet :(
    bx.scrollTo(xdata.value-(8*xd), ydata.value-(8*yd));
  }
  */
};

function ColorPickMouseMove(aEvent)
{
  viewer.captureColor(aEvent);
}

function ColorPickMouseDown()
{
  viewer.stopPickColor();
}

function ImageScrollMouseMove(aEvent)
{
  viewer.doScroll(aEvent);
}

function ImageScrollMouseUp(aEvent)
{
  viewer.stopScroll(aEvent);
}
