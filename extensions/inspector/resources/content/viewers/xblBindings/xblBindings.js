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
* XBLBindings --------------------------------------------
*  The viewer for the computed css styles on a DOM element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;
var gInitContent = false;

//////////// global constants ////////////////////

const kDOMDataSourceIID    = "@mozilla.org/rdf/datasource;1?name=Inspector_DOM";
const kXBLNSURI = "http://www.mozilla.org/xbl";

//////////////////////////////////////////////////

window.addEventListener("load", XBLBindings_initialize, false);
window.addEventListener("load", XBLBindings_destroy, false);

function XBLBindings_initialize()
{
  viewer = new XBLBindings();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

function XBLBindings_destroy()
{
  viewer.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class XBLBindings

function XBLBindings()
{
  this.mURL = window.location;
  this.mObsMan = new ObserverManager(this);
  this.mDOMUtils = XPCU.createInstance("@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

  this.mContentOutliner = document.getElementById("olContent");
  this.mContentOutlinerBody = document.getElementById("olbContent");
  this.mMethodOutliner = document.getElementById("olMethods");
  this.mPropOutliner = document.getElementById("olProps");
  this.mHandlerOutliner = document.getElementById("olHandlers");
  this.mResourceOutliner = document.getElementById("olResources");
  this.mFunctionTextbox = document.getElementById("txbFunction");
  
  if (gInitContent)
    this.initContent();
}

XBLBindings.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,
  mContentInit: false,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "xblBindings" },
  get pane() { return this.mPane },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mSubject = aObject;
    
    this.populateBindings();
    
    var menulist = document.getElementById("mlBindings");
    this.displayBinding(menulist.value);
        
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;

    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
    // persist all attributes on the panels
    var panes = document.getElementsByTagName("multipanel");
    for (var i = 0; i < panes.length; ++i) {
      InsUtil.persistAll(panes[i].id);
    }
  },
  
  initContent: function()
  {
    if (this.mContentInit) return;

    window.setTimeout(function(me) {
      // prepare and attach the content DOM datasource
      me.mContentDS = XPCU.createInstance(kDOMDataSourceIID, "inIDOMDataSource");
      me.mContentDS.removeFilterByType(3);
      me.mContentOutlinerBody.database.AddDataSource(me.mContentDS);
      
      // initialize the outliner builder for displaying anonymous content
      var tb = new inOutlinerBuilder(me.mContentOutliner, kInspectorNSURI, "Child");
      tb.rowFields = {"nodeType": "nodeType"};
      tb.initialize();
      tb.addColumn({ name: "nodeName", title: "", flex: 1, className: ""});
      tb.addColumn({ name: "nodeValue", title: "", flex: 1, className: ""});
      tb.build();
      
      me.mContentInit = true;
      
      if (me.mBinding)
        me.displayContent();
    }, 10, this);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// displaying binding info
  
  populateBindings: function()
  {
    var urls = this.mDOMUtils.getBindingURLs(this.mSubject);

    var popup = document.getElementById("mpBindings");
    this.clearChildren(popup);
    
    while (urls.hasMoreElements()) {
      var item = urls.getNext();
      var url = item.QueryInterface(Components.interfaces.nsIAtom).GetUnicode();
      var item = document.createElement("menuitem");
      item.setAttribute("value", url);
      item.setAttribute("label", url);
      popup.appendChild(item);
    }
    
    var menulist = document.getElementById("mlBindings");
    menulist.label = "";
    if (!popup.childNodes.length)
      menulist.value = "";
    else
      menulist.value = popup.childNodes[0].getAttribute("value");
  },
  
  displayBinding: function(aURL)
  {
    if (aURL) {
      var doc = document.implementation.createDocument(null, "", null);
      doc.addEventListener("load", gDocLoadListener, true);
      doc.load(aURL, "text/xml");
    
      this.mBindingDoc = doc;
      this.mBindingURL = aURL;
    } else {
      this.mBindingDoc = null;
      this.mBindingURL = null;
      this.doDisplayBinding();
    }
  },
  
  doDisplayBinding: function()
  {
    if (this.mBindingDoc) {
      var url = this.mBindingURL;
      var poundPt = url.indexOf("#");
      var id = url.substr(poundPt+1);
      var bindings = this.mBindingDoc.getElementsByTagName("binding");
      var binding = null;
      for (var i = 0; i < bindings.length; ++i) {
        if (bindings[i].getAttribute("id") == id) {
          binding = bindings[i];
          break;
        }
      }
      this.mBinding = binding;
    } else {
      this.mBinding = null;
    }
        
    this.displayContent();
    this.displayMethods();
    this.displayProperties();
    this.displayHandlers();
    this.displayResources();

    var ml = document.getElementById("mlBindings");    
    var mps = document.getElementById("mpsBinding");
    if (!this.mBinding) {
      ml.setAttribute("disabled", "true");
      mps.setAttribute("collapsed", "true");
    } else {
      ml.removeAttribute("disabled");
      mps.removeAttribute("collapsed");
    }
  },
  
  displayContent: function()
  {
    if (this.mContentInit && this.mBinding) {
      this.mContentDS.document = this.mBindingDoc;
      var list = this.mBinding.getElementsByTagName("content");
      if (list.length) {
        var res = this.mContentDS.getResourceForObject(list[0]);
        this.mContentOutlinerBody.setAttribute("ref", res.Value);
      } else {
        // no content, so clear the outliner
        this.mContentDS.document = null;
        this.mContentOutlinerBody.builder.rebuild();
      }
      document.getElementById("bxContent").setAttribute("disabled", list.length == 0);
    } 
  },
  
  displayMethods: function()
  {
    var bx = this.getBoxObject(this.mMethodOutliner);
    bx.view = this.mBinding ? new MethodOutlinerView(this.mBinding) : null;

    var active = this.mBinding && this.mBinding.getElementsByTagName("method").length > 0;
    document.getElementById("bxMethods").setAttribute("disabled", !active);
  },
  
  displayProperties: function()
  {
    var bx = this.getBoxObject(this.mPropOutliner);
    bx.view = this.mBinding ? new PropOutlinerView(this.mBinding) : null;

    var active = this.mBinding && this.mBinding.getElementsByTagName("property").length > 0;
    document.getElementById("bxProps").setAttribute("disabled", !active);

    this.displayProperty(null);
  },

  displayHandlers: function()
  {
    var bx = this.getBoxObject(this.mHandlerOutliner);
    bx.view = this.mBinding ? new HandlerOutlinerView(this.mBinding) : null;

    var active = this.mBinding && this.mBinding.getElementsByTagName("handler").length > 0;
    document.getElementById("bxHandlers").setAttribute("disabled", !active);
    this.displayHandler(null);
  },

  displayResources: function()
  {
    var bx = this.getBoxObject(this.mResourceOutliner);
    bx.view = this.mBinding ? new ResourceOutlinerView(this.mBinding) : null;

    var active = this.mBinding && this.mBinding.getElementsByTagName("resources").length > 0;
    document.getElementById("bxResources").setAttribute("disabled", !active);
  },

  displayMethod: function(aMethod)
  {
    var body = aMethod.getElementsByTagNameNS(kXBLNSURI, "body")[0];
    if (body) {
      this.mFunctionTextbox.value = this.readDOMText(body);
    }
  },

  displayProperty: function(aProp)
  {
    var getradio = document.getElementById("raPropGetter");
    var setradio = document.getElementById("raPropSetter");
    
    // disable/enable radio buttons
    var hasget = aProp && (aProp.hasAttribute("onget") || aProp.getElementsByTagName("getter").length);
    getradio.setAttribute("disabled", !hasget);
    if (!hasget && getradio.checked)
      getradio.removeAttribute("checked");
    var hasset = aProp && (aProp.hasAttribute("onset") || aProp.getElementsByTagName("setter").length);
    setradio.setAttribute("disabled", !hasset);
    if (!hasset && setradio.checked)
      setradio.removeAttribute("checked");
    
    // make sure at least one is checked
    if (!setradio.checked && !getradio.checked) {
      if (!getradio.disabled) 
        getradio.setAttribute("checked", "true");
      else if (!setradio.disabled)
        setradio.setAttribute("checked", "true");
    }
    
    // display text
    var et = getradio.hasAttribute("checked") ? "get" : setradio.hasAttribute("checked") ? "set" : null;
    var text = "";
    if (!et || !aProp) {
      // do nothing
    } else if (aProp.hasAttribute("on"+et))
      text = aProp.getAttribute("on"+et);
    else {
      var kids = aProp.getElementsByTagName(et+"ter")
      if (kids && kids.length) {
        text = this.readDOMText(kids[0]);
      }
    }
    this.mFunctionTextbox.value = text;
  },
  
  displayHandler: function(aHandler)
  {
    var text = "";
    if (aHandler) {
      text = aHandler.hasAttribute("action") ? aHandler.getAttribute("action") : null;
      if (!text)
        text = this.readDOMText(aHandler);
    }
    this.mFunctionTextbox.value = text;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// selection
  
  onMethodSelected: function()
  {
    var idx = this.mMethodOutliner.currentIndex;
    var methods = this.mBinding.getElementsByTagNameNS(kXBLNSURI, "method");
    var method = methods[idx];
    this.displayMethod(method);
  },
    
  onPropSelected: function()
  {
    var idx = this.mPropOutliner.currentIndex;
    var props = this.mBinding.getElementsByTagNameNS(kXBLNSURI, "property");
    var prop = props[idx];
    this.displayProperty(prop);
  },
    
  onHandlerSelected: function()
  {
    var idx = this.mHandlerOutliner.currentIndex;
    var handlers = this.mBinding.getElementsByTagNameNS(kXBLNSURI, "handler");
    var handler = handlers[idx];
    this.displayHandler(handler);
  },
    
  ////////////////////////////////////////////////////////////////////////////
  //// misc
  
  clearChildren: function(aEl)
  {
    var kids = aEl.childNodes;
    for (var i = kids.length-1; i >=0; --i)
      aEl.removeChild(kids[i]);
  },
  
  readDOMText: function(aEl)
  {
    var text = aEl.nodeValue;
    for (var i = 0; i < aEl.childNodes.length; ++i) {
      text += this.readDOMText(aEl.childNodes[i]);
    }
    return text;
  },

  getBoxObject: function(aOutliner)
  {
    return aOutliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject);
  }
  
};

////////////////////////////////////////////////////////////////////////////
//// MethodOutlinerView

function MethodOutlinerView(aBinding)
{
  this.mMethods = aBinding.getElementsByTagNameNS(kXBLNSURI, "method");
  this.mRowCount = this.mMethods ? this.mMethods.length : 0;
}

MethodOutlinerView.prototype = new inBaseOutlinerView();

MethodOutlinerView.prototype.getCellText = 
function(aRow, aColId) 
{
  var method = this.mMethods[aRow];
  if (aColId == "olcMethodName") {
    var name = method.getAttribute("name");
    var params = method.getElementsByTagName("parameter");
    var pstr = "";
    for (var i = 0; i < params.length; ++i)
      pstr += (i>0?", ": "") + params[i].getAttribute("name");
    return name + "(" + pstr + ")";
  }
  
  return "";
}

////////////////////////////////////////////////////////////////////////////
//// PropOutlinerView

function PropOutlinerView(aBinding)
{
  this.mProps = aBinding.getElementsByTagNameNS(kXBLNSURI, "property");
  this.mRowCount = this.mProps ? this.mProps.length : 0;
}

PropOutlinerView.prototype = new inBaseOutlinerView();

PropOutlinerView.prototype.getCellText = 
function(aRow, aColId) 
{
  var prop = this.mProps[aRow];
  if (aColId == "olcPropName") {
    return prop.getAttribute("name");
  }
  
  return "";
}

////////////////////////////////////////////////////////////////////////////
//// HandlerOutlinerView

function HandlerOutlinerView(aBinding)
{
  this.mHandlers = aBinding.getElementsByTagNameNS(kXBLNSURI, "handler");
  this.mRowCount = this.mHandlers ? this.mHandlers.length : 0;
}

HandlerOutlinerView.prototype = new inBaseOutlinerView();

HandlerOutlinerView.prototype.getCellText = 
function(aRow, aColId) 
{
  var handler = this.mHandlers[aRow];
  if (aColId == "olcHandlerEvent") {
    return handler.getAttribute("event");
  } else if (aColId == "olcHandlerPhase") {
    return handler.getAttribute("phase");
  }
  
  return "";
}

function gDocLoadListener()
{
  viewer.doDisplayBinding();
}

////////////////////////////////////////////////////////////////////////////
//// ResourceOutlinerView

function ResourceOutlinerView(aBinding)
{
  var res = aBinding.getElementsByTagNameNS(kXBLNSURI, "resources");
  var list = [];
  if (res && res.length) {
    var kids = res[0].childNodes;
    for (var i = 0; i < kids.length; ++i)
      if (kids[i].nodeType == 1)
        list.push(kids[i]);
  }
        
  this.mResources = list;
  this.mRowCount = this.mResources ? this.mResources.length : 0;
}

ResourceOutlinerView.prototype = new inBaseOutlinerView();

ResourceOutlinerView.prototype.getCellText = 
function(aRow, aColId) 
{
  var resource = this.mResources[aRow];
  if (aColId == "olcResourceType") {
    return resource.localName;
  } else if (aColId == "olcResourceSrc") {
    return resource.getAttribute("src");
  }
  
  return "";
}

////////////////////////////////////////////////////////////////////////////
//// event listeners

function gDocLoadListener()
{
  viewer.doDisplayBinding();
}