/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the UDDI Inquiry API
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

//
// Global JS console helper functions
//

var gConsoleService = Components.classes['@mozilla.org/consoleservice;1'].getService(Components.interfaces.nsIConsoleService);

function logUDDIError(aMessage, aSourceName, aSourceLine)
{
  var errorObject = Components.classes['@mozilla.org/scripterror;1'].createInstance(Components.interfaces.nsIScriptError);
  if (gConsoleService && errorObject) {
    errorObject.init(aMessage, aSourceName, aSourceLine, 0, 0, 0, "UDDI SOAP");
    gConsoleService.logMessage(errorObject);
  }
}

function logUDDIWarning(aMessage, aSourceName, aSourceLine, aFlag)
{
  var errorObject = Components.classes['@mozilla.org/scripterror;1'].createInstance(Components.interfaces.nsIScriptError);
  if (gConsoleService && errorObject) {
    errorObject.init(aMessage, aSourceName, aSourceLine, 0, 0, 1, "UDDI SOAP");
    gConsoleService.logMessage(errorObject);
  }
}

function logUDDIMessage(aMessage)
{
  if (gConsoleService)
    gConsoleService.logStringMessage(aMessage);
}

//
// UDDI Inquiry proxy class
//

const kEnvelopeBegin = "<?xml version='1.0' encoding='utf-8'?><Envelope xmlns='http://schemas.xmlsoap.org/soap/envelope/'><Body>";
const kEnvelopeEnd = "</Body></Envelope>";


function UDDIInquiry()
{
  this.mXMLHttpRequest = new XMLHttpRequest();
  this.decoder = new UDDIDecoder();
  this.encoder = new UDDIEncoder();
}

UDDIInquiry.prototype = 
{
  operatorSite    : null,
  mXMLHttpRequest : null,
  serializer      : null,
  decoder         : null,
  encoder         : null,
  
  findBinding : function (aServiceKey, aMaxRows, aFindQualifiers, aTModelBag)
  {
    var msg = this.encoder.encodeFindBinding(aServiceKey, aMaxRows, aFindQualifiers, aTModelBag);
    this.sendRequest(msg);
    return this.decoder.decodeBindingDetail(this.mXMLHttpRequest.responseXML);
  },

  findBusiness : function (aMaxRows, aFindQualifiers, aName, aIdentifierBag, aCategoryBag, aTModelBag, aDiscoveryURLs)
  {
    var msg = this.encoder.encodeFindBusiness(aName, aFindQualifiers, aIdentifierBag, aCategoryBag, aTModelBag, aDiscoveryURLs, aMaxRows);
    this.sendRequest(msg);
    return this.decoder.decodeBusinessList(this.mXMLHttpRequest.responseXML);
  },

  findRelatedBusinesses : function (aMaxRows, aFindQualifiers, aBusinessKey, aKeyedReference)
  { 
    var msg = this.encoder.encodeFindRelatedBussinesses(aMaxRows, aFindQualifiers, aBusinessKey, aKeyedReference);
    this.sendRequest(msg);
    return this.decoder.decodeRelatedBusinessesList(this.mXMLHttpRequest.responseXML);
  },

  findService : function (aBusinessKey, aMaxRows, aFindQualifiers, aName, aCategoryBag, aTModelBag)
  { 
    var msg = this.encoder.encodeFindService(aName, aFindQualifiers, aCategoryBag, aTModelBag, aMaxRows, aBusinessKey);
    this.sendRequest(msg);
    return this.decoder.decodeServiceList(this.mXMLHttpRequest.responseXML);
  },

  findTModel : function (aMaxRows, aFindQualifiers, aName, aIdentifierBag, aCategoryBag)
  { 
    var msg = this.encoder.encodeFindTModel(aMaxRows, aFindQualifiers, aName, aIdentifierBag, aCategoryBag);
    this.sendRequest(msg);
    return this.decoder.decodeTModelList(this.mXMLHttpRequest.responseXML);
  },

  getBindingDetail : function (aBindingKey)
  {
    var msg = this.encoder.encodeGetBindingDetail(aBindingKey);
    this.sendRequest(msg);
    return this.decoder.decodeBindingDetail(this.mXMLHttpRequest.responseXML);
  },

  getBusinessDetail : function (aBusinessKey)
  {
    var msg = this.encoder.encodeGetBusinessDetail(aBusinessKey);
    this.sendRequest(msg);
    return this.decoder.decodeBusinessDetail(this.mXMLHttpRequest.responseXML);
  },

  getBusinessDetailExt : function (aBusinessKey)
  {
    var msg = this.encoder.encodeGetBusinessDetailExt(aBusinessKey);
    this.sendRequest(msg);
    return this.decoder.decodeBusinessDetailExt(this.mXMLHttpRequest.responseXML);
  },

  getServiceDetail : function (aServiceKey)
  {
    var msg = this.encoder.encodeGetServiceDetail(aServiceKey);
    this.sendRequest(msg);
    return this.decoder.decodeServiceDetail(this.mXMLHttpRequest.responseXML);
  },

  getTModelDetail : function (aTModelKey)
  {
    var msg = this.encoder.encodeGetTModelDetail(aTModelKey);
    this.sendRequest(msg);
    return this.decoder.decodeTModelDetail(this.mXMLHttpRequest.responseXML);
  },

  sendRequest : function (aMessage)
  {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
    this.mXMLHttpRequest.open("POST", this.operatorSite, false);

    this.mXMLHttpRequest.setRequestHeader("Content-Type","text/xml; charset=\"utf-8\"");
    this.mXMLHttpRequest.setRequestHeader("SOAPAction","\"\"");

    this.mXMLHttpRequest.overrideMimeType("text/xml");
    
    this.mXMLHttpRequest.send(aMessage);
  },

  // for debugging purposes
  getResponseMessage : function ()
  {
    if (!this.serializer)
      this.serializer = new XMLSerializer();
    return this.serializer.serializeToString(this.mXMLHttpRequest.responseXML);
  }
}; // end of class UDDIInquiry

function UDDIEncoder() {
}

UDDIEncoder.prototype = 
{

  //-----------------------------------------------------------------------------
  // Encoding of UDDI Inquiry API find_XX calls - WSDL/alpha order
  //-----------------------------------------------------------------------------

  encodeFindBinding : function (aServiceKey, aMaxRows, aFindQualifiers, aTModelBag)
  {
    // check required elements and types
    if ((!aServiceKey) ||
        (!aTModelBag || !(aTModelBag instanceof TModelBag)) ||
        (aFindQualifiers && !(aFindQualifiers instanceof FindQualifiers))) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return null;
	  }

    var rv = kEnvelopeBegin;
    rv += "<find_binding xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" ";
    rv += "serviceKey=\"" + aServiceKey + "\" ";

    if (aMaxRows)
      rv += "maxRows=\"" + aMaxRows + "\" ";
    rv += ">";

    if (aFindQualifiers)
      rv += this.encodeFindQualifiers(aFindQualifiers);
    if (aTModelBag)
      rv += this.encodeTModelBag(aTModelBag);

    rv += "</find_binding>";
    rv += kEnvelopeEnd;

    return rv;
  },

  encodeFindBusiness : function (aName, aFindQualifiers, aIdentifierBag, aCategoryBag, aTModelBag, aDiscoveryURLs, aMaxRows)
  {
    // check types, no required elements
    if ((aFindQualifiers && !(aFindQualifiers instanceof FindQualifiers)) ||
        (aIdentifierBag && !(aIdentifierBag instanceof IdentifierBag))    ||
        (aCategoryBag && !(aCategoryBag instanceof CategoryBag))          ||
        (aTModelBag && !(aTModelBag instanceof TModelBag))) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return null; // report error;
    }

    var rv = kEnvelopeBegin;
    rv += "<find_business xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" ";

    if (aMaxRows)
      rv += "maxRows=\"" + aMaxRows + "\" ";
    rv += ">";

    if (aFindQualifiers)
      rv += this.encodeFindQualifiers(aFindQualifiers);
    if (aName)
      rv += this.encodeName(aName);
    if (aIdentifierBag)
      rv += this.encodeIdentifierBag(aIdentifierBag);
    if (aCategoryBag)
      rv += this.encodeCategoryBag(aCategoryBag);
    if (aTModelBag)
      rv += this.encodeTModelBag(aTModelBag);
    if (aDiscoveryURLs)
      rv += this.encodeDiscoveryURLs(aDiscoveryURLs);

    rv += "</find_business>";
    rv += kEnvelopeEnd;

    return rv;
  },

  encodeFindRelatedBussinesses : function (aMaxRows, aFindQualifiers, aBusinessKey, aKeyedReference)
  {
    // check required elements and types
    if (!aBusinessKey ||
        (aFindQualifiers && !(aFindQualifiers instanceof FindQualifiers)) ||
        (aKeyedReference && !(aKeyedReference instanceof KeyedReference))) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return null; // XXX report error;
    }

    var rv = kEnvelopeBegin;
    rv += "<find_relatedBusinesses xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" ";

    if (aMaxRows)
      rv += "maxRows=\"" + aMaxRows + "\" ";
    rv += ">";

    if (aFindQualifiers)
      rv += this.encodeFindQualifiers(aFindQualifiers);
    rv += "<businessKey>" + aBusinessKey + "</businessKey>";
    if (aKeyedReference)
      rv += this.encodeKeyedReference(aKeyedReference);

    rv += "</find_relatedBusinesses>";
    rv += kEnvelopeEnd;

    return rv;
  },

  encodeFindService : function (aName, aFindQualifiers, aCategoryBag, aTModelBag, aMaxRows, aBusinessKey)
  {
    if ((aFindQualifiers && !(aFindQualifiers instanceof FindQualifiers)) ||
        (aCategoryBag && !(aCategoryBag instanceof CategoryBag))          ||
        (aTModelBag && !(aTModelBag instanceof TModelBag))) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return null; // report error;
    }

    var rv = kEnvelopeBegin;
    rv += "<find_service xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" ";

    if (aMaxRows)
      rv += "maxRows=\"" + aMaxRows + "\" ";
    if (aBusinessKey)
      rv += "businessKey=\"" + aBusinessKey + "\" ";
    rv += ">";

    if (aFindQualifiers)
      rv += this.encodeFindQualifiers(aFindQualifiers);
    if (aName)
      rv += "<name>" + aName + "</name>";
    if (aCategoryBag)
      rv += this.encodeCategoryBag(aCategoryBag);
    if (aTModelBag)
      rv += this.encodeTModelBag(aTModelBag);

    rv += "</find_service>";
    rv += kEnvelopeEnd;

    return rv;
  },

  encodeFindTModel : function (aMaxRows, aFindQualifiers, aName, aIdentifierBag, aCategoryBag)
  {
    if ((aFindQualifiers && !(aFindQualifiers instanceof FindQualifiers)) ||
        (aIdentifierBag && !(aIdentifierBag instanceof IdentifierBag))    ||
        (aCategoryBag && !(aCategoryBag instanceof CategoryBag))) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return null; // report error;
    }

    var rv = kEnvelopeBegin;
    rv += "<find_tModel xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" ";

    if (aMaxRows)
      rv += "maxRows=\"" + aMaxRows + "\" ";
    rv += ">";

    if (aFindQualifiers)
      rv += this.encodeFindQualifiers(aFindQualifiers);
    if (aName)
      rv += "<name>" + aName + "</name>";
    if (aIdentifierBag)
      rv += this.encodeIdentifierBag(aIdentifierBag);
    if (aCategoryBag)
      rv += this.encodeCategoryBag(aCategoryBag);

    rv += "</find_tModel>";
    rv += kEnvelopeEnd;

    return rv;
  },

  //-----------------------------------------------------------------------------
  // Encoding of Inquiry UDDI API get_XX calls - alpha order
  //-----------------------------------------------------------------------------

  encodeGetBindingDetail : function (aBindingKey)
  {
    var rv = kEnvelopeBegin;
    rv += "<get_bindingDetail xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" >";
    rv += "<bindingKey>" + aBindingKey + "</bindingKey>";
    rv += "</get_bindingDetail>";
    rv += kEnvelopeEnd;
    return rv;
  },

  encodeGetBusinessDetail : function (aBusinessKey)
  {
    var rv = kEnvelopeBegin;
    rv += "<get_businessDetail xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" >";
    rv += "<businessKey>" + aBusinessKey + "</businessKey>";
    rv += "</get_businessDetail>";
    rv += kEnvelopeEnd;
    return rv;
  },

  encodeGetBusinessDetailExt : function (aBusinessKey)
  {
    var rv = kEnvelopeBegin;
    rv += "<get_businessDetailExt xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" >";
    rv += "<businessKey>" + aBusinessKey + "</businessKey>";
    rv += "</get_businessDetailExt>";
    rv += kEnvelopeEnd;
    return rv;
  },

  encodeGetServiceDetail : function (aServiceKey)
  {
    var rv = kEnvelopeBegin;
    rv += "<get_serviceDetail xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" >";
    rv += "<serviceKey>" + aServiceKey + "</serviceKey>";
    rv += "</get_serviceDetail>";
    rv += kEnvelopeEnd;
    return rv;
  },

  encodeGetTModelDetail : function (aTModelKey)
  {
    var rv = kEnvelopeBegin;
    rv += "<get_tModelDetail xmlns=\"urn:uddi-org:api_v2\" generic=\"2.0\" >";
    rv += "<tModelKey>" + aTModelKey + "</tModelKey>";
    rv += "</get_tModelDetail>";
    rv += kEnvelopeEnd;
    return rv;
  },

  //-----------------------------------------------------------------------------
  // Encoding of UDDI Spec types used as Inquiry call arguements - alpha sort
  //-----------------------------------------------------------------------------

  /* */
  encodeCategoryBag : function (aCategoryBag)
  {
    var rv = "";
    if (aCategoryBag) {
      var krobjs = aCategoryBag.keyedReferences;
      var length = krobjs.length;
      if (length > 0) {
        rv = "<categoryBag>";
        rv += this.encodeKeyedReferences(krobjs);
        rv += "</categoryBag>";
      }
    }
    return rv;
  },

  /* */
  encodeDiscoveryURLs : function (aDiscoveryURLs)
  {
    var rv = "";
    if (aDiscoveryURLs) {
      var durlobjs = aDiscoveryURLs;
      var length  = durlobjs.length;
      if (length > 0) {
        rv = "<discoveryURLs>";
        for (var i = 0; i < length; i++) {
          var durl = durlobjs[i];
          if (durl && !(durl instanceof DiscoveryURL)) {
            logUDDIError("Bad DiscoveryURLs arguement", "UDDIEncode.js", "encodeDiscoveryURLS()");
            return null; // XXX report error
          }
          rv += "<discoveryURL useType=\"" + durl.useType + ">";
          rv += durl.stringValue;
          rv += "</discoveryURL>";
        }
        rv += "</discoveryURLs>";
      }
    }
    return rv;
  },

  /* */
  encodeFindQualifiers : function (aFindQualifiers)
  {
    var rv = "";
    if (aFindQualifiers) {
      rv = "<findQualifiers>"; // can be empty
      if (aFindQualifiers.findQualifers != null) {
        var fqstrs = aFindQualifiers.findQualifiers;
        var length = fqstrs.length;
        if (length > 0) {
          for (var i = 0; i < length; i++)
            rv += "<findQualifier>" + fqstrs[i] + "</findQualifier>";
        }
      }
      rv += "</findQualifiers>";
    }
    return rv;
  },

  /* */
  encodeIdentifierBag : function (aIdentifierBag)
  {
    var rv = "";
    if (aIdentifierBag) {
      var krobjs = aIdentifierBag.keyedReferences;
      var length = krobjs.length;
      if (length > 0) {
        rv = "<identifierBag>";
        rv += this.encodeKeyedReferences(krobjs);
        rv += "</identifierBag>";
      }
    }
    return rv;
  },

  /* */
  encodeKeyedReference : function (aKeyedReference)
  {
    if (aKeyedReference && 
        (!(aKeyedReference instanceof KeyedReference) || 
         !aKeyedReference.keyValue)) {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      return ""; // XXX report error -- do we want to type check on ALL encode methods?
    }

    var rv = "<keyedReference ";
    if (aKeyedReference.tModelKey)
      rv += "tModelKey=\"" + aKeyedReference.tModelKey + "\" ";
    if (aKeyedReference.keyName)
      rv += "keyName=\"" + aKeyedReference.keyName + "\" ";
    if (aKeyedReference.keyValue)
      rv += "keyValue=\"" + aKeyedReference.keyValue + "\" ";
    rv += "/>";
  },

  // XXX just creates an array, move to caller?
  /* */
  encodeKeyedReferences : function (aKeyedReferences)
  {
    var rv = "";
    if (aKeyedReferences) {
      var length = aKeyedReferences.length;
      for (var i = 0; i < length; i++) {
        rv += this.encodeKeyedReference(aKeyedReference);
      }
    }
    return rv;
  },

  /* */
  encodeName : function (aName)
  {
    var rv = "";
    if (typeof aName == "string")
      rv = "<name>" + aName + "</name>";
    else if (aName.constructor == Array) {
      var length = aName.length;
      for (var i = 0; i < length; i++) {
        rv += "<name>" + aName[i] + "</name>";
      }
    }
    else {
      logUDDIError("Bad Arguements to UDDIInquiry.findBinding()", "UDDIEncode.js", "EncodeFindBinding()");
      // XXX report error
    }
    return rv;
  },

  /* */
  encodeTModelBag : function (aTModelBag)
  {
    var rv = "";
    if (aTModelBag) {
      var tmkstrs = aTModelBag.tModelKeys;
      var length  = tmkstrs.length;
      if (length > 0) {
        rv = "<tModelBag>";
        for (var i = 0; i < length; i++) {
          rv += "<tModelKey>" + tmkstrs[i] + "</tModelKey>";
        }
        rv += "</tModelBag>";
      }
    }
    return rv;
  }
}; // end of class UDDIEncoder