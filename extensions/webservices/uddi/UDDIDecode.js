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
// This whole file is the UDDI Decoder class. It's purpose is to decode from a
//   DOM document the different types returned by UDDI servers. It returns the
//   proper types based on the API and the uddi.xsd file available at 
//   http://www.uddi.org. The types are defined in js in UDDITypes.js
//

function UDDIDecoder() 
{
  this.debug = true;
  this.verbose = true;
}

UDDIDecoder.prototype = 
{
  debug     : null,
  verbose   : null,
  
  //-----------------------------------------------------------------------------
  // Helper methods
  //-----------------------------------------------------------------------------

  getChildElementByName : function (aName, aParent)
  {
    var node = aParent.firstChild;
    while (node) {
      if (node.nodeType == Node.ELEMENT_NODE && 
          node.nodeName === (aName))
        return node;
      node = node.nextSibling;
    }
    return null;
  },

  getChildElementsByName : function (aName, aParent)
  {
    var rv = null;
    var node = aParent.firstChild;
    while (node) {
      if (node.nodeType == Node.ELEMENT_NODE && 
          node.nodeName === (aName)) {
        if (!rv) 
          rv = new Array();
        rv.push(node);
      }
      node = node.nextSibling;
    }
    return rv;
  },

  //-----------------------------------------------------------------------------
  // UDDI Inquiry API find_xx return type decoders
  //-----------------------------------------------------------------------------

  /* also called by get_xx method */
  decodeBindingDetail : function (aDocument)
  {
    // the BindingDetail should be the first child of the SOAP Body
    var bindingDetailElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!bindingDetailElement) {
      if (this.verbose)
        logUDDIError("No BindingDetail element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBindingDetail()");
      return null;
    }

    var bindingDetail = new BindingDetail();

    // set attributes - only truncated is *optional*
    bindingDetail.generic = bindingDetailElement.getAttribute("generic");
    bindingDetail.operator = bindingDetailElement.getAttribute("operator");
    bindingDetail.truncated = bindingDetailElement.getAttribute("truncated");

    // populate the *optional* BindingTemplate array
    var bindTemplList = this.getChildElementsByName("bindingTemplate", bindingDetailElement);
    if (bindTemplList)
      bindingDetail.bindingTemplates = this.decodeBindingTemplateArray(bindTemplList);

    // check for presence of required fields
    if (!bindingDetail.generic ||
        !bindingDetail.operator) {
      logUDDIError("BindingDetail missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBindingDetail()");
      return null;
    }

    return bindingDetail;
  },

  /* */
  decodeBusinessList : function (aDocument)
  {
    // the BusinessList should be the first child of the SOAP Body
    var busListElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!busListElement) {
      logUDDIError("No BusinessList element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessList()");
      return null;
    }

    var busList = new BusinessList();

    // set attributes - only truncated is *optional*
    busList.generic = busListElement.getAttribute("generic");
    busList.operator = busListElement.getAttribute("operator");
    busList.truncated = busListElement.getAttribute("truncated");

    // populate *required* BusinessInfo array
    var busInfosElement = this.getChildElementByName("businessInfos", busListElement);
    var busInfoList = this.getChildElementsByName("businessInfo", busInfosElement);
    var length = busInfoList ? busInfoList.length : 0;
    if (length > 0) {
      busList.businessInfos = new Array(length);
      for (var i = 0; i < length; i++)
        busList.businessInfos[i] = this.decodeBusinessInfo(busInfoList[i]);
    }
  
    // check for presence of required fields
    if (!busList.generic ||
        !busList.operator ||
        !busList.businessInfos) {
      logUDDIError("BusinessList missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessList()");
      return null;
    }

    return busList;
  },

  /* */
  decodeRelatedBusinessesList : function (aDocument)
  {
    // the RelatedBusinessList should be the first child of the SOAP Body
    var relBusListElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!relBusListElement) {
      logUDDIError("No RelatedBusinessesList element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeRelatedBusinessesList()");
      return null;
    }

    var relBusList = new RelatedBusinessesList();

    // set attributes - only truncated is *optional*
    relBusList.generic = relBusListElement.getAttribute("generic");
    relBusList.operator = relBusListElement.getAttribute("operator");
    relBusList.truncated = relBusListElement.getAttribute("truncated");
 
    // set *required* BusinessKey
    relBusList.businessKey = this.getChildElementByName("businessKey", relBusListElement).firstChild.nodeValue;

    // populate RelatedBusinessInfo Array - *optional*, but the RBInfos _tag_ is *required*
    var relBusInfosElement = this.getChildElementsByName("relatedBusinessInfos", relBusListElement);
    if (!relBusInfosElement) {
      logUDDIWarning("RelatedBusinessesList missing required data member. Bad UDDI Response from server. Continuing.", "UDDIDecode.js", "decodeRelatedBusinessesList()");
    }
    var relBusInfoList = this.getChildElementsByName("relatedBusinessInfo", relBusInfosElement);
    var length = relBusInfoList ? relBusInfoList.length : 0;
    if (length > 0) {
      relBusList.relatedBusinessInfos = new Array(length);
      for (var i = 0; i < length; i++)
        relBusList.relatedBusinessInfos[i] = this.decodeRelatedBusinessInfo(relBusInfoList[i]);
    }

    // check for presence of required fields
    if (!relBusList.generic ||
        !relBusList.operator ||
        !relBusList.businessKey) {
      logUDDIError("RelatedBusinessesList missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeRelatedBusinessesList()");
      return null;
    }

    return relBusList;
  },

  /* */
  decodeServiceList : function (aDocument)
  {
    // the ServiceList should be the first child of the SOAP Body
    var serviceListElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!serviceListElement) {
      logUDDIError("No ServiceList element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeServiceList()");
      return null;
    }

    var serviceList = new ServiceList();

    // set attributes - only truncated is *optional* 
    serviceList.generic = serviceListElement.getAttribute("generic");
    serviceList.operator = serviceListElement.getAttribute("operator");
    serviceList.truncated = serviceListElement.getAttribute("truncated");

    // populate *required* ServiceInfo array
    var serviceInfosElement= this.getChildElementByName("serviceInfos", serviceListElement);
    var serviceInfoList = this.getChildElementsByName("serviceInfo", serviceInfosElement);
    var length = serviceInfoList ? serviceInfoList.length : 0;
    if (length > 0) {
      serviceList.serviceInfos = new Array(length);
      for (var i = 0; i < length; i++)
        serviceList.serviceInfos[i] = this.decodeServiceInfo(serviceInfoList[i]);
    }
     
    // check for presence of required fields
    if (!serviceList.generic ||
        !serviceList.operator ||
        !serviceList.serviceInfos) {
      logUDDIError("ServiceList missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeServiceList()");
      return null;
    }

    return serviceList;
  },

  /* */
  decodeTModelList : function (aDocument)
  {
    // the tModelList should be the first child of the SOAP Body
    var tModelListElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!tModelListElement) {
      logUDDIError("No TModelList element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelList()");
      return null;
    }

    var tModelList = new TModelList();

    // set attributes - only truncated is *optional*  
    tModelList.generic = tModelListElement.getAttribute("generic");
    tModelList.operator = tModelListElement.getAttribute("operator");
    tModelList.truncated = tModelListElement.getAttribute("truncated");
  
    // populate *required* TModelInfo array
    var tModelInfosElement = this.getChildElementByName("tModelInfos", tModelListElement);
    var tModelInfoList = this.getChildElementsByName("tModelInfo", tModelInfosElement);
    var length = tModelInfoList ? tModelInfoList.length : 0;
    if (length > 0) {
      tModelList.tModelInfos = new Array(length);
      for (var i = 0; i < length; i++)
        tModelList.tModelInfos[i] = this.decodeTModelInfo(tModelInfoList[i]);
    }
  
    // check for presence of required fields
    if (!tModelList.generic ||
        !tModelList.operator ||
        !tModelList.tModelInfos) {
      logUDDIError("TModelList missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelList()");
      return null;
    }

    return tModelList;
  },

  //-----------------------------------------------------------------------------
  // UDDI Inquiry API get_xx return type decoders
  //-----------------------------------------------------------------------------

  /* */
  decodeBusinessDetail : function (aDocument)
  {
    // the BusinessDetail should be the first child of the SOAP Body
    var busDetailElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!busDetailElement) {
      logUDDIError("No BusinessDetail element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessDetail()");
      return null;
    }

    var busDetail = new BusinessDetail();

    // set attributes - only truncated is *optional*
    busDetail.generic = busDetailElement.getAttribute("generic");
    busDetail.operator = busDetailElement.getAttribute("operator");
    busDetail.truncated = busDetailElement.getAttribute("truncated");

    // populate *optional* BusinessEntity array
    var busEntityList = this.getChildElementsByName("businessEntity", busDetailElement);
    var length = busEntityList ? busEntityList.length : 0;
    if (length > 0) {
      busDetail.businessEntities = new Array(length);
      for (var i = 0; i < length; i++)
        busDetail.businessEntities[i] = this.decodeBusinessEntity(busEntityList[i]);
    }
  
    // check for presence of required fields
    if (!busDetail.generic ||
        !busDetail.operator) {
      logUDDIError("BusinessDetail missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessDetail()");
      return null;
    }

    return busDetail;
  },

  /* */
  decodeBusinessDetailExt : function (aDocument)
  {
    // the BusinessDetailExt should be the first child of the SOAP Body
    var busDetailExtElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!busDetailExtElement) {
      logUDDIError("No BusinessDetailExt element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessDetailExt()");
      return null;
    }

    var busDetailExt = new BusinessDetailExt();

    // set attributes - only truncated is *optional*
    busDetailExt.generic = busDetailExtElement.getAttribute("generic");
    busDetailExt.operator = busDetailExtElement.getAttribute("operator");
    busDetailExt.truncated = busDetailExtElement.getAttribute("truncated");

    // populate *required* BusinessEntity array
    var busEntityExtList = this.getChildElementsByName("businessEntityExt", busDetailExtElement);
    var length = busEntityExtList ? busEntityExtList.length : 0;
    if (length > 0) {
      busDetailExt.businessEntityExts = new Array(length);
      for (var i = 0; i < length; i++)
        busDetailExt.businessEntityExts[i] = this.decodeBusinessEntityExt(busEntityExtList[i]);
    }

    // check for presence of required fields
    if (!busDetailExt.generic ||
        !busDetailExt.operator ||
        !busDetailExt.businessEntityExts) {
      logUDDIError("BusinessDetailExt missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessDetailExt()");
      return null;
    }

    return busDetailExt;
  },

  /* */
  decodeServiceDetail : function (aDocument)
  {
    // the ServiceDetail should be the first child of the SOAP Body
    var serviceDetailElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!serviceDetailElement) {
      logUDDIError("No ServiceDetail element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeServiceDetail()");
      return null;
    }

    var serviceDetail = new ServiceDetail();

    // set attributes - only truncated is *optional*
    serviceDetail.generic = serviceDetailElement.getAttribute("generic");
    serviceDetail.operator = serviceDetailElement.getAttribute("operator");
    serviceDetail.truncated = serviceDetailElement.getAttribute("truncated");

    // populate *optional* BusinessService array
    var busServiceList = this.getChildElementsByName("businessService", serviceDetailElement);
    if (busServiceList)
      serviceDetail.businessServices = this.decodeBusinessServiceArray(busServiceList);

    // check for presence of required fields
    if (!serviceDetail.generic ||
        !serviceDetail.operator) {
      logUDDIError("ServiceDetail missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeServiceDetail()");
      return null;
    }

    return serviceDetail;
  },

  /* */
  decodeTModelDetail : function (aDocument)
  {
    // the TModelDetail should be the first child of the SOAP Body
    var tModelDetailElement = this.getChildElementByName("Body", aDocument.documentElement).firstChild;
    if (!tModelDetailElement) {
      logUDDIError("No TModelDetail element to decode. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelDetail()");
      return null;
    }

    var tModelDetail = new TModelDetail();

    // set attributes - only truncated is *optional*
    tModelDetail.generic = tModelDetailElement.getAttribute("generic");
    tModelDetail.operator = tModelDetailElement.getAttribute("operator");
    tModelDetail.truncated = tModelDetailElement.getAttribute("truncated");

    // populate *required* TModel array
    var tModelList = this.getChildElementsByName("tModel", tModelDetailElement);
    var length = tModelList ? tModelList.length : 0;
    if (length > 0) {
      tModelDetail.tModels = new Array(length);
      for (var i = 0; i < length; i++)
        tModelDetail.tModels[i] = this.decodeTModel(tModelList[i]);
    }
  
    // check for presence of required fields
    if (!tModelDetail.generic ||
        !tModelDetail.operator ||
        !tModelDetail.tModels) {
      logUDDIError("TModelDetail missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelDetail()");
      return null;
    }
  
    return tModelDetail;
  },

  //-----------------------------------------------------------------------------
  // Decoding of UDDI Spec types - alpha sort
  //-----------------------------------------------------------------------------

  /* */
  decodeAccessPoint : function (aAccessPointElement)
  {
    if (!aAccessPointElement) {
      logUDDIWarning("No AccessPoint element.", "UDDIDecode.js", "decodeAccessPoint()");
      return null;
    }

    var accessPoint = new AccessPoint();

    // set *required* attribute
    accessPoint.urlType = aAccessPointElement.getAttribute("URLType");

    // set *required* stringValue
    accessPoint.stringValue = aAccessPointElement.firstChild.nodeValue;

    // check for presence of required fields
    if (!accessPoint.urlType || !accessPoint.stringValue) {
      logUDDIError("AccessPoint missing a required field. Bad UDDI Response", "UDDIDecode.js", "decodeAccess()");
      return null;
    }

    return accessPoint;
  },

  /* */
  decodeAddress : function (aAddressElement)
  {
    if (!aAddressElement) {
      logUDDIWarning("No Address element.", "UDDIDecode.js", "decodeAddress()");
      return null;
    }

    var address = new Address();

    // set attributes (all optional)
    address.useType = aAddressElement.getAttribute("useType");
    address.sortCode = aAddressElement.getAttribute("sortCode");
    address.tModelKey = aAddressElement.getAttribute("tModelKey");

    // populate the *optional* AddressLine array
    var addLineList = this.getChildElementsByName("addressLine", aAddressElement);
    var length = addLineList ? addLineList.length : 0;
    if (length > 0) {
      address.addressLines = new Array(length);
      for (var i = 0; i < length; i++)
        address.addressLines[i] = this.decodeAddressLine(addLineList[i]);
    }

    // no required fields, just return
    return address;
  },

  /* */
  decodeAddressLine : function (aAddLineElement)
  {
    if (!aAddLineElement) {
      logUDDIWarning("No AddressLine element.", "UDDIDecode.js", "decodeAddressLine()");
      return null;
    }

    var addLine = new AddressLine();

    // set attributes (all optional)
    addLine.keyName = aAddLineElement.getAttribute("keyName");
    addLine.keyValue = aAddLineElement.getAttribute("keyValue");

    // set *required* string value
    addLine.stringValue = aAddLineElement.firstChild.nodeValue;

    // check for presence of required fields
    if (!addLine.stringValue) {
      logUDDIError("AddressLine missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeAddressLine()");
      return null;
    }

    return addLine;
  },

  /* will return an empty array */
  decodeBindingTemplateArray : function (aBindTemplElList)
  {
    var length = aBindTemplElList ? aBindTemplElList.length : 0;
    var bindTemplArray = new Array(length);
    if (length > 0) {
      for (var i = 0; i < length; i++)
        bindTemplArray[i] = this.decodeBindingTemplate(aBindTemplElList[i]);
    }
    else
      logUDDIWarning("BindingTemplate List is null or empty.", "UDDIDecode.js", "decodeBindingTemplateArray()");
    return bindTemplArray;
  },

  /* */
  decodeBindingTemplate : function (aBindingTemplateElement)
  {
    if (!aBindingTemplateElement) {
      logUDDIWarning("No BindingTemplate element.", "UDDIDecode.js", "decodeBindingTemplate()");
      return null;
    }

    var bindingTemplate = new BindingTemplate();

    // set attributes - bindingKey is *required*
    bindingTemplate.serviceKey = aBindingTemplateElement.getAttribute("serviceKey");
    bindingTemplate.bindingKey = aBindingTemplateElement.getAttribute("bindingKey");

    // set *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aBindingTemplateElement);
    bindingTemplate.descriptions = this.decodeDescriptionArray(descriptionList);

    // set *required* AccessPoint
    var accessPointElement = this.getChildElementByName("accessPoint", aBindingTemplateElement);
    bindingTemplate.accessPoint = this.decodeAccessPoint(accessPointElement);

    // set *required* HostingRedirector
    var hostingRedirectorElement = this.getChildElementByName("hostingRedirector", aBindingTemplateElement);
    bindingTemplate.hostingRedirector = this.decodeHostingRedirector(hostingRedirectorElement);

    // set *required* TModelInstanceDetails (array of TModelInstanceInfo objects) (can be empty)
    var tModelInstanceDetailsElement = this.getChildElementByName("tModelInstanceDetails", aBindingTemplateElement);
    if (tModelInstanceDetailsElement) {
      var tModelInstanceInfoList = this.getChildElementsByName("tModelInstanceInfo", tModelInstanceDetailsElement);
      var length = tModelInstanceInfoList ? tModelInstanceInfoList.length : 0;
      bindingTemplate.tModelInstanceDetails = new Array(length);
      if (length > 0) {
        for (var i = 0; i < length; i++)
          bindingTemplate.tModelInstanceDetails[i] = this.decodeTModelInstanceInfo(tModelInstanceInfoList[i]);
      }
    }

    // check for presence of required fields
    if (!bindingTemplate.bindingKey ||
        (!bindingTemplate.accessPoint && !bindingTemplate.hostingRedirector) || // choice
        !bindingTemplate.tModelInstanceDetails) {
      logUDDIError("BindingTemplate missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBindingTemplate()");
      return null;
    }

    return bindingTemplate;
  },

  /* */
  decodeBusinessEntity : function (aBusEntityElement)
  {
    if (!aBusEntityElement) {
      logUDDIWarning("No BusinessEntity element.", "UDDIDecode.js", "decodeBusinessEntity()");
      return null;
    }

    var busEntity = new BusinessEntity();

    // set attributes - businessKey is *required*
    busEntity.businessKey = aBusEntityElement.getAttribute("businessKey");
    busEntity.operator = aBusEntityElement.getAttribute("operator");
    busEntity.authorizedName = aBusEntityElement.getAttribute("authorizedName");

    // populate *optional* DiscoveryURL array (can NOT be empty)
    var discURLsElement = this.getChildElementByName("discoveryURLs", aBusEntityElement);
    if (discURLsElement) {
      var discURLList = this.getChildElementsByName("discoveryURL", discURLsElement);
      var length = discURLList ? discURLList.length : 0;
      if (length > 0) {
        busEntity.discoveryURLs = new Array(length);
        for (var i = 0; i < length; i++)
          busEntity.discoveryURLs[i] = this.decodeDiscoveryURL(discURLList[i]);
      }
    }

    // populate *required* Name array
    var nameList = this.getChildElementsByName("name", aBusEntityElement);
    busEntity.names = this.decodeNameArray(nameList);

    // populate *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aBusEntityElement);
    busEntity.descriptions = this.decodeDescriptionArray(descriptionList);

    // populate *optional* Contact array (can NOT be empty)
    var contactsElement = this.getChildElementByName("contacts", aBusEntityElement);
    if (contactsElement) {
      var contactList = this.getChildElementsByName("contact", contactsElement);
      length = contactList ? contactList.length : 0;
      if (length > 0) {
        busEntity.contacts = new Array(length);
        for (var i = 0; i < length; i++)
          busEntity.contacts[i] = this.decodeContact(contactList[i]);
      }
    }

    // populate *optional* BusinessServices array (can be empty)
    var businessServicesElement = this.getChildElementByName("businessServices", aBusEntityElement);
    if (businessServicesElement) {
      var businessServiceElementList = this.getChildElementsByName("businessService", businessServicesElement);
      busEntity.businessServices = this.decodeBusinessServiceArray(businessServiceElementList);
    }

    // decode *optional* IdentifierBag
    var identBagElement = this.getChildElementByName("identifierBag", aBusEntityElement);
    busEntity.identifierBag = this.decodeIdentifierBag(identBagElement);

    // decode *optional* CategoryBag
    var catBagElement = this.getChildElementByName("categoryBag", aBusEntityElement);
    busEntity.categoryBag = this.decodeCategoryBag(catBagElement);

    // check for presence of required fields
    if (!busEntity.names ||
        !busEntity.businessKey) {
      logUDDIError("BusinessEntity missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessEntity()");
      return null;
    }

    return busEntity;
  },

  /* */
  decodeBusinessEntityExt : function (aBusinessEntityExtElement)
  {
    if (!aBusinessEntityExtElement) {
      logUDDIWarning("No BusinessEntityExt element.", "UDDIDecode.js", "decodeBusinessEntityExt()");
      return null;
    }

    var busEntityExt = new BusinessEntityExt();

    // set *required* BusinessEntity
    var busEntityElement = this.getChildElementByName("businessEntity", aBusinessEntityExtElement);
    busEntityExt.businessEntity = this.decodeBusinessEntity(busEntityElement);

    // set *optional* extension array of XML elements
    var sibling = busEntityElement.nextSibling;
    if (sibling) {
      busDetailExt.extensions = new Array();
      while (sibling) {
        busDetailExt.extensions.push(sibling);
        sibling = sibling.nextSibling;
      }
    }

    if (!busEntityExt.businessEntity) {
      logUDDIError("BusinessEntityExt missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessEntityExt()");
      return null;
    }

    return busEntityExt;
  },


  /* */
  decodeBusinessInfo : function (aBusinessInfoElement)
  {
    if (!aBusinessInfoElement) {
      logUDDIWarning("No BusinessInfo element.", "UDDIDecode.js", "decodeBusinessInfo()");
      return null;
    }

    var busInfo = new BusinessInfo();

    // set *required* attribute
    busInfo.businessKey = aBusinessInfoElement.getAttribute("businessKey");

    // set *required* Name array
    var nameList = this.getChildElementsByName("name", aBusinessInfoElement);
    busInfo.names = this.decodeNameArray(nameList);

    // set *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aBusinessInfoElement);
    busInfo.descriptions = this.decodeDescriptionArray(descriptionList);

    // populate *required* ServiceInfo array (can be empty)
    var serviceInfosElement = this.getChildElementByName("serviceInfos", aBusinessInfoElement);
    if (serviceInfosElement) {
      var serviceInfoList = this.getChildElementsByName("serviceInfo", serviceInfosElement);
      var length = serviceInfoList ? serviceInfoList.length : 0;
      busInfo.serviceInfos = new Array(length);
      if (length > 0) {
        for (var i = 0; i < length; i++)
          busInfo.serviceInfos[i] = this.decodeServiceInfo(serviceInfoList[i]);
      }
    }

    // check for presence of required fields
    if (!busInfo.businessKey ||
        !busInfo.names ||
        !busInfo.serviceInfos) {
      logUDDIError("BusinessInfo missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessInfo()");
      return null;
    }

    return busInfo;
  },

  // will return an empty array if arg is null/empty
  decodeBusinessServiceArray : function (aBusinessServiceElementList)
  {
    var length = aBusinessServiceElementList ? aBusinessServiceElementList.length : 0;
    var businessServiceArray = new Array(length);
    if (length > 0) {
      for (var i = 0; i < length; i++)
        businessServiceArray [i] = this.decodeBusinessService(aBusinessServiceElementList[i]);
    }
    else
      logUDDIMessage("DescriptionArray is empty.");
    return businessServiceArray;
  },

  /* */
  decodeBusinessService : function (aBusServiceElement)
  {
    if (!aBusServiceElement) {
      logUDDIWarning("No BusinessService element.", "UDDIDecode.js", "decodeBusinessService()");
      return null;
    }

    var busService = new BusinessService();

    // set attributes - only serviceKey is *required*
    busService.serviceKey = aBusServiceElement.getAttribute("serviceKey");
    busService.businessKey = aBusServiceElement.getAttribute("businessKey");

    // populate *optional* Name array  
    var nameList = this.getChildElementsByName("name", aBusServiceElement);
    busService.names = this.decodeNameArray(nameList);

    // populate *optional* Description array  
    var descList = this.getChildElementsByName("description", aBusServiceElement);
    busService.descriptions = this.decodeDescriptionArray(descList);

    // populate *optional* BindingTemplate array  (can be empty)
    var bindTemplsElement = this.getChildElementByName("bindingTemplates", aBusServiceElement);
    if (bindTemplsElement) {
      var bindTemplList = this.getChildElementsByName("bindingTemplate", bindTemplsElement);
      busService.bindingTemplates = this.decodeBindingTemplateArray(bindTemplList);
    }

    // set the *optional* CategoryBag
    var catBagElement = this.getChildElementsByName("categoryBag", aBusServiceElement);
    busService.categoryBag = this.decodeCategoryBag(catBagElement);

    // check for presence of required fields
    if (!busService.serviceKey) {
      logUDDIError("BusinessService missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeBusinessService()");
      return null;
    }

    return busService;
  },

  /* */
  decodeCategoryBag : function (aCategoryBagElement)
  {
    if (!aCategoryBagElement) {
      logUDDIWarning("No CategoryBag element.", "UDDIDecode.js", "decodeCategoryBag()");
      return null;
    }

    var categoryBag = new CategoryBag();

    // populate the *required* KeyedReference array
    var keyedReferenceList = this.getChildElementsByName("keyedReference", aCategoryBagElement);
    categoryBag.keyedReferences = this.decodeKeyedRefArray(keyedReferenceList);

    // check for presence of required field
    if (!categoryBag.keyedReferences) {
      logUDDIError("CategoryBag missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeCategoryBag()");
      return null;
    }

    return categoryBag;
  },

  /* */
  decodeContact : function (aContactElement)
  {
    if (!aContactElement) {
      logUDDIWarning("No Contact element.", "UDDIDecode.js", "decodeContact()");
      return null;
    }

    var contact = new Contact();

    // set *optional* attribute
    contact.useType = aContactElement.getAttribute("useType");

    // populate *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aContactElement);
    contact.descriptions = this.decodeDescriptionArray(descriptionList);

    // set *required* personName - string
    contact.personName = this.getChildElementByName("personName", aContactElement).firstChild.nodeValue;

    // populate *optional* Phone array
    var phoneList = this.getChildElementsByName("phone", aContactElement);
    var length = phoneList ? phoneList.length : 0;
    if (length > 0) {
      contact.phones = new Array(length);
      for (var i = 0; i < length; i++)
        contact.phones[i] = this.decodePhone(phoneList[i]);
    }

    // populate *optional* Email array
    var emailList = this.getChildElementsByName("email", aContactElement);
    length = emailList ? emailList.length : 0;
    if (length > 0) {
      contact.emails = new Array(length);
      for (var i = 0; i < length; i++)
        contact.emails[i] = this.decodeEmail(emailList[i]);
    }

    // populate *optional* Address array
    var addressList = this.getChildElementsByName("address", aContactElement);
    length = addressList ? addressList.length : 0;
    if (length > 0) {
      contact.addresses = new Array(length);
      for (var i = 0; i < length; i++)
        contact.addresses[i] = this.decodeAddress(addressList[i]);
    }

    // check for presence of required field
    if (!contact.personName) {
      logUDDIError("Contact missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeContact()");
      return null;
    }

    return contact;
  },

  /* */
  // will NOT return an empty array
  decodeDescriptionArray : function (aDescElementList)
  {
    var length = aDescElementList ? aDescElementList.length : 0;
    if (length > 0) {
      var descArray = new Array(length);
      for (var i = 0; i < length; i++)
        descArray[i] = this.decodeDescription(aDescElementList[i]);
      return descArray;
    }
    logUDDIMessage("DescriptionArray is empty.");
    return null;
  },

  /* */
  decodeDescription : function (aDescriptionElement)
  {
    if (!aDescriptionElement) {
      logUDDIWarning("No Description element.", "UDDIDecode.js", "decodeDescription()");
      return null;
    }

    var desc = new Description();

    // set *optional* attribute
    desc.lang = aDescriptionElement.getAttribute("lang");

    // set *required* string value
    desc.stringValue = aDescriptionElement.firstChild.nodeValue;

    // check for presence of required field
    if (!desc.stringValue) {
      logUDDIError("Description missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeDescription()");
      return null;
    }

    return desc;
  },

  /* */
  decodeDiscoveryURL : function (aDiscoveryURLElement)
  {
    if (!aDiscoveryURLElement) {
      logUDDIWarning("No DiscoveryURL element.", "UDDIDecode.js", "decodeDiscoveryURL()");
      return null;
    }

    var discURL = new DiscoveryURL();

    // set *required* attribute
    discURL.useType = aDiscoveryURLElement.getAttribute("useType");

    // set *required* string value
    discURL.stringValue = aDiscoveryURLElement.firstChild.nodeValue;

    // check for presence of required field
    if (!discURL.useType ||
        !discURL.stringValue) {
      logUDDIError("DiscoveryURL missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeDiscoveryURL()");
      return null;
    }

    return discURL;
  },

  /* */
  decodeEmail : function (aEmailElement)
  {
    if (!aEmailElement) {
      logUDDIWarning("No Email element.", "UDDIDecode.js", "decodeEmails()");
      return null;
    }

    var email = new Email();

    // set *optional* attribute
    email.useType = aEmailElement.getAttribute("useType");

    // set *required* string value
    email.stringValue = aEmailElement.firstChild.nodeValue;

    // check for presence of required field
    if (!email.stringValue) {
      logUDDIError("Email missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeEmail()");
      return null;
    }

    return email;
  },

  /* */
  decodeHostingRedirector : function (aHostingRedirectorElement)
  {
    if (!aHostingRedirectorElement) {
      logUDDIWarning("No HostingRedirector element.", "UDDIDecode.js", "decodeHostingRedirector()");
      return null;
    }

    var hostRedir = new HostingRedirector();

    // set *required* attribute
    hostRedir.bindingKey = aHostingRedirector.getAttribute("bindingKey");

    // check for presence of required field
    if (!hostRedir.bindingKey) {
      logUDDIError("HostingRedirector missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeHostingRedirector()");
      return null;
    }

    return hostRedir;
  },

  /* */
  decodeIdentifierBag : function (aIdentifierBagElement)
  {
    if (!aIdentifierBagElement) {
      logUDDIWarning("No IdentifierBag element.", "UDDIDecode.js", "decodeIdentifierBag()");
      return null;
    }

    var identifierBag = new IdentifierBag();

    // populate the *required* KeyedReference array
    var keyedReferenceList = this.getChildElementsByName("keyedReference", aIdentifierBagElement);
    identifierBag.keyedReferences = this.decodeKeyedRefArray(keyedReferenceList);

    // check for presence of required field
    if (!identifierBag.keyedReferences) {
      logUDDIError("IdentifierBag missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeIdentifierBag()");
      return null;
    }

    return identifierBag;
  },

  /* */
  decodeInstanceDetails : function (aInstanceDetailsElement)
  {
    if (!aInstanceDetailsElement) {
      logUDDIWarning("No InstanceDetails element.", "UDDIDecode.js", "decodeInstanceDetails()");
      return null;
    }

    var instanceDetails = new InstanceDetails(); 

    // populate *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aInstanceDetailsElement);
    instanceDetails.descriptions = this.decodeDescriptionArray(descriptionList);

    // set *optional* OverviewDoc
    var overviewDocElement = this.getChildElementByName("overviewDoc", aInstanceDetailsElement);
    instanceDetails.overviewDoc = this.decodeOverviewDoc(overviewDocElement);

    // set *optional* InstanceParms - string
    var instanceParmsElement = this.getChildElementByName("instanceParms", aInstanceDetailsElement);
    if (instanceParmsElement)
      instanceDetails.instanceParms = instanceParmsElement.firstChild.nodeValue;

    // no required fields, just return
    return instanceDetails;
  },

  /* */
  // will NOT return an empty array - all KeyedReference arrays are currently *required*
  decodeKeyedRefArray : function (aKRElementList)
  {
    var length = aKRElementList ? aKRElementList.length : 0;
    if (length > 0) {
      var krArray = new Array(length);
      for (var i = 0; i < length; i++)
        krArray[i] = this.decodeKeyedReference(aKRElementList[i]);
      return krArray;
    }
    logUDDIWarning("KeyedReference List is null or empty.", "UDDIDecode.js", "decodeKeyedRefArray()");
    return null;  
  },

  /* */
  decodeKeyedReference : function (aKRElement)
  {
    if (!aKRElement) {
      logUDDIWarning("No KeyedReference element.", "UDDIDecode.js", "decodeKeyedReference()");
      return null;
    }

    var keyedRef = new KeyedReference();

    // set attributes - keyValue is *required*
    keyedRef.tModelKey = aKRElement.getAttribute("tModelKey");
    keyedRef.keyName = aKRElement.getAttribute("keyName");
    keyedRef.keyValue = aKRElement.getAttribute("keyValue");

    // check for presence of required field
    if (!keyedRef.keyValue) {
      logUDDIError("KeyedReference missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeKeyedReference()");
      return null;
    }

    return keyedRef;
  },

  // does NOT return an empty array!
  /* */
  decodeNameArray : function (aNameElementList)
  {
    var length = aNameElementList ? aNameElementList.length : 0;
    if (length > 0) {
      var nameArray = new Array(length);
      for (var i = 0; i < length; i++)
        nameArray[i] = this.decodeName(aNameElementList[i]);
      return nameArray;
    }
    logUDDIWarning("Name List is null or empty.", "UDDIDecode.js", "decodeNameArray()");
    return null;
  },

  /* */
  decodeName : function (aNameElement)
  {
    if (!aNameElement) {
      logUDDIWarning("No Name element.", "UDDIDecode.js", "decodeName()");
      return null;
    }

    var name = new Name();

    // set *optional* attribute
    name.lang = aNameElement.getAttribute("lang");

    // set *required* string value
    name.stringValue = aNameElement.firstChild.nodeValue;

    // check for the presence of required field
    if (!name.stringValue) {
      logUDDIError("Name missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeName()");
      return null;
    }

    return name;
  },

  /* */
  decodeOverviewDoc : function (aOverviewDocElement)
  {
    if (!aOverviewDocElement) {
      logUDDIWarning("No OverviewDoc element.", "UDDIDecode.js", "decodeOverviewDoc()");
      return null;
    }

    var overviewDoc = new OverviewDoc();

    // populate *optional* Description array
    var descriptionList = this.getChildElementsByName("description", aOverviewDocElement); 
    overviewDoc.descriptions = this.decodeDescriptionArray(descriptionList);

    // set *optional* OverviewURL strings
    var overviewURLElement = this.getChildElementByName("overviewURL", aOverviewDocElement);
    if (overviewURLElement)
      overviewDoc.overviewURL = overviewURLElement.firstChild.nodeValue;

    // no required fields, just return  
    return overviewDoc;
  },

  /* */
  decodePhone : function (aPhoneElement)
  {
    if (!aPhoneElement) {
      logUDDIWarning("No Phone element.", "UDDIDecode.js", "decodePhone()");
      return null;
    }

    var phone = new Phone();

    // set *optional* attribute
    phone.useType = aPhoneElement.getAttribute("useType");

    // set *required* string value
    phone.strinValue = aPhoneElement.firstChild.nodeValue;

    // check for presence of required field
    if (!phone.stringValue) {
      logUDDIError("Phone missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodePhone()");
      return null;
    }

    return phone;
  },

  /* */
  decodeRelatedBusinessInfo : function (aRelatedBusinessInfoElement)
  {
    if (!aRelatedBusinessInfoElement) {
      logUDDIWarning("No RelatedBusinessInfo element.", "UDDIDecode.js", "decodeRelatedBusinessInfo()");
      return null;
    }

    var relBusInfo = new RelatedBusinessInfo();

    // set *required* businessKey
    relBusInfo.businessKey = this.getChildElementByName("businessKey", aRelatedBusinessInfoElement).firstChild.nodeValue;

    // populate *required* Names array
    var nameList = this.getChildElementsByName("name", aRelatedBusinessInfoElement);
    relBusInfo.names = this.decodeNameArray(nameList);

    // populate *optional* Description array
    var descList = this.getChildElementsByName("description", aRelatedBusinessInfoElement);
    relBusInfo.descriptions = this.decodeDescriptionArray(descList);

    // set *required* SharedRelationships array
    var shRelsList = this.getChildElementsByName("sharedRelationships", aRelatedBusinessInfoElement);
    var length = shRelsList ? shRelsList.length : 0;
    if (length > 0 && length <= 2) {
      relBusInfo.sharedRelationships = new Array(length);
      for (var i = 0; i < length; i++)
        relBusInfo.sharedRelationships[i] = this.decodeSharedRelationships(shRelsList[i]);
    }

    // check for presence of required fields
    if (!relBusInfo.businessKey||
        !relBusInfo.names ||
        !relBusInfo.sharedRelationships) {
      logUDDIError("RelatedBusinessInfo missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeRelatedBusinessInfo()");
      return null;
    }

    return relBusInfo;
  },

  /* */
  decodeServiceInfo : function (aServiceInfoElement)
  {
    if (!aServiceInfoElement) {
      logUDDIWarning("No ServiceInfo element.", "UDDIDecode.js", "decodeServiceInfo()");
      return null;
    }

    var serviceInfo = new ServiceInfo();

    // set attributes - all are *required*
    serviceInfo.serviceKey = aServiceInfoElement.getAttribute("serviceKey");
    serviceInfo.businessKey = aServiceInfoElement.getAttribute("businessKey");

    // set *optional* Name array
    var nameList = this.getChildElementsByName("name", aServiceInfoElement);
    serviceInfo.names = this.decodeNameArray(nameList);

    // check for presence of required fields
    if (!serviceInfo.serviceKey ||
        !serviceInfo.businessKey) {
      logUDDIError("ServiceInfo missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeServiceInfo()");
      return null;
    }

    return serviceInfo;
  },

  /* */
  decodeSharedRelationships : function (aSharedRelationshipsElement)
  {
    if (!aSharedRelationshipsElement) {
      logUDDIWarning("No SharedRelationships element.", "UDDIDecode.js", "decodeSharedRelationships()");
      return null;
    }

    var sharedRelationships = new SharedRelationships();

    // set *required* attribute
    sharedRelationships.direction = aSharedRelationshipsElement.getAttribute("direction");

    // set *required* KeyedReference array
    var keyedRefList = this.getChildElementsByName("keyedReference", aSharedRelationshipsElement);
    sharedRelationships.keyedReferences = this.decodeKeyedRefArray(keyedRefList);

    if (!sharedRelationships.direction ||
        !sharedRelationships.keyedReferences) {
      logUDDIError("SharedRelationships missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeSharedRelationships()");
      return null;
    }

    return sharedRelationships;
  },

  /* */
  decodeTModel : function (aTModelElement)
  {
    if (!aTModelElement) {
      logUDDIWarning("No TModel element.", "UDDIDecode.js", "decodeTModel()");
      return null;
    }

    var tModel = new TModel();

    // set attributes - only tModelKey is *required*
    tModel.tModelKey = aTModelElement.getAttribute("tModelKey");
    tModel.operator = aTModelElement.getAttribute("operator");
    tModel.authorizedName = aTModelElement.getAttribute("authorizedName");

    // set *required* Name
    tModel.name = this.getChildElementByName("name", aTModelElement).firstChild.nodeValue;

    // populate *optional* Description array
    var descList = this.getChildElementsByName("description", aTModelElement);
    tModel.descriptions = this.decodeDescriptionArray(descList);

    // populate *optional* OverviewDoc
    var overviewDocElement = this.getChildElementByName("overviewDoc", aTModelElement);
    tModel.overviewDoc = this.decodeDescriptionArray(overviewDocElement);

    // populate *optional* IdentifierBag
    var identBagElement = this.getChildElementByName("identifierBag", aTModelElement);
    tModel.identifierBag = this.decodeIdentifierBag(identBagElement);

    // populate *optional* CategoryBag
    var catBagElement = this.getChildElementByName("categoryBag", aTModelElement);
    tModel.categoryBag = this.decodeCategoryBag(catBagElement);

    // check for presence of required fields
    if (!tModel.name ||
        !tModel.tModelKey) {
      logUDDIError("TModel missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModel()");
      return null;
    }

    return tModel;
  },

  /* */
  decodeTModelInfo : function (aTModelInfoElement)
  {
    if (!aTModelInfoElement) {
      logUDDIWarning("No TModelInfo element.", "UDDIDecode.js", "decodeTModelInfo()");
      return null;
    }

    var tModelInfo = new TModelInfo();

    // set *required* attribute
    tModelInfo.tModelKey = aTModelInfoElement.getAttribute("tModelKey");

    // set *required* Name
    tModelInfo.name = this.getChildElementByName("name", aTModelInfoElement).firstChild.nodeValue;

    // check for presence of required fields
    if (!tModelInfo.tModelKey||
        !tModelInfo.name) {
      logUDDIError("TModelInfo missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelInfo()");
      return null;
    }

    return tModelInfo;
  },

  /* */
  decodeTModelInstanceInfo : function (aTModelInstanceInfoElement)
  {
    if (!aTModelInstanceInfoElement) {
      logUDDIWarning("No TModelInstanceInfo element.", "UDDIDecode.js", "decodeTModelInstanceInfo()");
      return null;
    }

    var tModelInstanceInfo = new TModelInstanceInfo();

    // set *required* attribute
    tModelInstanceInfo.tModelKey = aTModelInstanceInfoElement.getAttribute("tModelKey");

    // set *optional* Description array
    var descList = this.getChildElementsByName("description", aTModelInstanceInfoElement);
    tModelInstanceInfo.descriptions = this.decodeDescriptionArray(descList);

    // set *optional* InstanceDetails
    var instanceDetailsElement = this.getChildElementByName("instanceDetails", aTModelInstanceInfoElement);
    tModelInstanceInfo.instanceDetails = this.decodeInstanceDetails(instanceDetailsElement);

    // check for presence of required field
    if (!tModelInstanceInfo.tModelKey) {
      logUDDIError("TModelInstanceInfo missing required data member. Bad UDDI Response from server.", "UDDIDecode.js", "decodeTModelInstanceInfo()");
      return null;
    }

    return tModelInstanceInfo;
  }

}; // end of UDDIDecoder class
