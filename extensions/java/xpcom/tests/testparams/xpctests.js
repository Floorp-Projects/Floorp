/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***********************************************************
    constants
 ***********************************************************/

const nsISupports = Components.interfaces.nsISupports;
const nsIEcho = Components.interfaces.nsIEcho;
const nsIXPCTestIn = Components.interfaces.nsIXPCTestIn;
const nsIXPCTestArray = Components.interfaces.nsIXPCTestArray;
const nsIJXTestArrayParams = Components.interfaces.nsIJXTestArrayParams;

const CLASS_ID = Components.ID("{9e45a36d-7cf7-4f2a-a415-d0b07e54945b}");
const CLASS_NAME = "XPConnect Tests in Javascript";
const CONTRACT_ID = "@mozilla.org/javaxpcom/tests/xpc;1";

/***********************************************************
    class definition
 ***********************************************************/

//class constructor
function XPCTests() {
};

// class definition
XPCTests.prototype = {

  // ----------------------------------
  //    nsISupports
  // ----------------------------------
  
  QueryInterface: function(aIID) {
    if (!aIID.equals(nsIEcho) &&
        !aIID.equals(nsIXPCTestIn) &&
        !aIID.equals(nsIXPCTestArray) &&
        !aIID.equals(nsIJXTestArrayParams) &&
        !aIID.equals(nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // ----------------------------------
  //    nsIEcho
  // ----------------------------------

  In2OutOneString: function(input) {
    return input;
  },

  In2OutOneDOMString: function(input) {
    return input;
  },

  In2OutOneAString: function(input) {
    return input;
  },

  In2OutOneUTF8String: function(input) {
    return input;
  },

  In2OutOneCString: function(input) {
    return input;
  },

  _string: null,

  SetAString: function(str) {
    _string = str;
  },

  GetAString: function() {
    return _string;
  },

  // ----------------------------------
  //    nsIXPCTestIn
  // ----------------------------------

  EchoLong: function(l) {
    return l;
  },

  EchoShort: function(a) {
    return a;
  },

  EchoChar: function(c) {
    return c;
  },

  EchoBoolean: function(b) {
    return b;
  },

  EchoOctet: function(o) {
    return o;
  },

  EchoLongLong: function(ll) {
    return ll;
  },

  EchoUnsignedShort: function(us) {
    return us;
  },

  EchoUnsignedLong: function(ul) {
    return ul;
  },

  EchoFloat: function(f) {
    return f;
  },

  EchoDouble: function(d) {
    return d;
  },

  EchoWchar: function(wc) {
    return wc;
  },

  EchoString: function(ws) {
    return ws;
  },

  EchoPRBool: function(b) {
    return b;
  },

  EchoPRInt32: function(l) {
    return l;
  },

  EchoPRInt16: function(l) {
    return l;
  },

  EchoPRInt64: function(i) {
    return i;
  },

  EchoPRUint8: function(i) {
    return i;
  },

  EchoPRUint16: function(i) {
    return i;
  },

  EchoPRUint32: function(i) {
    return i;
  },

  EchoPRUint64: function(i) {
    return i;
  },

  EchoVoid: function() {},

  // ----------------------------------
  //    nsIXPCTestArray
  // ----------------------------------

  MultiplyEachItemInIntegerArray : function(val, len, a) {
    for(var i = 0; i < len; i++)
      a.value[i] *= val;
  },
  
  DoubleStringArray : function(len, a) {
      var outArray = new Array();
      for(var i = 0; i < len.value; i++) {
          var inStr = a.value[i];
          var outStr = new String();
          for(var k = 0; k < inStr.length; k++) {
              outStr += inStr[k];
              outStr += inStr[k];
          }
          outArray.push(outStr);
          outArray.push(outStr);
      }
      len.value *= 2;
      a.value = outArray;
  },

  ReverseStringArray : function(len, a) {
      for(var i = 0; i < len/2; i++) {
          var temp = a.value[i];
          a.value[i] = a.value[len-1-i];
          a.value[len-1-i] = temp;
      }
  },

  // ----------------------------------
  //    nsIJXTestArrayParams
  // ----------------------------------

  multiplyEachItemInIntegerArray2 : function(val, a, len) {
    this.MultiplyEachItemInIntegerArray(val, len, a);
  },

  copyIntArray: function(srcArray, count, dstArray) {
    dstArray.value = srcArray.slice();
  },

  returnIntArray: function(srcArray, count) {
    return srcArray;
  },
  
  copyByteArray: function(srcArray, count, dstArray) {
    dstArray.value = srcArray.slice();
  },

  returnByteArray: function(srcArray, count) {
    return srcArray;
  },
  
  copySizedString: function(srcString, count, dstString) {
    dstString.value = srcString.slice();
  },
  
  returnSizedString: function(srcString, count) {
    return srcString;
  },
  
  copySizedWString: function(srcString, count, dstString) {
    dstString.value = srcString.slice();
  },
  
  returnSizedWString: function(srcString, count) {
    return srcString;
  }
};

/***********************************************************
    class factory
 ***********************************************************/
var XPCTestsFactory = {
  createInstance: function (aOuter, aIID)
  {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new XPCTests()).QueryInterface(aIID);
  }
};

/***********************************************************
    module definition (xpcom registration)
 ***********************************************************/
var XPCTestsModule = {
  _firstTime: true,
  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType)
  {
    aCompMgr = aCompMgr.
        QueryInterface(Components.interfaces.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(CLASS_ID, CLASS_NAME, 
        CONTRACT_ID, aFileSpec, aLocation, aType);
  },

  unregisterSelf: function(aCompMgr, aLocation, aType)
  {
    aCompMgr = aCompMgr.
        QueryInterface(Components.interfaces.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(CLASS_ID, aLocation);        
  },
  
  getClassObject: function(aCompMgr, aCID, aIID)
  {
    if (!aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    if (aCID.equals(CLASS_ID))
      return XPCTestsFactory;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload: function(aCompMgr)
  {
    return true;
  }
};

/***********************************************************
    module initialization
 ***********************************************************/
function NSGetModule(aCompMgr, aFileSpec)
{
  return XPCTestsModule;
}
