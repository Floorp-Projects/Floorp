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
 *                 John Gaunt <jgaunt@netscape.com>
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

////
//
// Type Definitions for UDDI Inquiry and Publish (eventually) calls. 
//

// KEY:
// shortcut-array: means that there is an actual element of the name of the
//     field that contains only an array of objects of the specified type.
//     To reduce the number of classes the container class has been removed.
// optional: this field does not need to be set
// required: this field must be set
// attribute: this field is represented by an attribute on the element
// unbounded: there can be multiple elements of this type contained in the
//      parent element.

// ----------------------------------------------------------------------------
// UDDI Inquiry Request Message types - alpha sort
// ----------------------------------------------------------------------------

/* encoder */
function Find_Binding() { }

Find_Binding.prototype = 
{
  findQualifiers         : null, // [optional] - shortcut-array of FindQualifier object (can be empty)
  tModelBag              : null, // [required]
  generic                : null, // [required, attribute]
  maxRows                : null, // [optional, attribute]
  serviceKey             : null, // [required, attribute]
}

/* encoder */
function Find_Business() { }

Find_Business.prototype = 
{
  findQualifiers         : null, // [optional] - shortcut-array of FindQualifier object (can be empty)
  names                  : null, // [optional, unbounded] - array of Name objects
  identifierBag          : null, // [optional]
  categoryBag            : null, // [optional]
  tModelBag              : null, // [optional]
  discoveryURLs          : null, // [optional, unbounded] - shortcut-array of DiscoveryURL objects (if present, cannot be empty)
  generic                : null, // [required, attribute]
  maxRows                : null, // [optional, attribute]
}

/* encoder */
function Find_RelatedBusinesses() { }

Find_RelatedBusinesses.prototype = 
{
  findQualifiers         : null, // [optional] - shortcut-array of FindQualifier object (can be empty)
  businessKey            : null, // [required] - string
  keyedReference         : null, // [optional]
  generic                : null, // [required, attribute]
  maxRows                : null, // [optional, attribute]
}

/* encoder */
function Find_Service() { }

Find_Service.prototype = 
{
  findQualifiers         : null, // [optional] - shortcut-array of FindQualifier object (can be empty)
  names                  : null, // [optional, unbounded] - array of Name objects
  categoryBag            : null, // [optional]
  tModelBag              : null, // [optional]
  generic                : null, // [required, attribute]
  maxRows                : null, // [optional, attribute]
  businessKey            : null, // [optional, attribute]
}

/* encoder */
function Find_TModel() { }

Find_TModel.prototype = 
{
  findQualifiers         : null, // [optional] - shortcut-array of FindQualifier object (can be empty)
  name                   : null, // [optional]
  identifierBag          : null, // [optional]
  categoryBag            : null, // [optional]
  generic                : null, // [required, attribute]
  maxRows                : null, // [optional, attribute]
}

/* encoder */
function Get_BindingDetail() { }

Get_BindingDetail.prototype = 
{
  bindingKeys            : null, // [required, unbounded] - array of bindingKey strings
  generic                : null, // [required, attribute]
}

/* encoder */
function Get_BusinessDetail() { }

Get_BusinessDetail.prototype = 
{
  businessKeys           : null, // [required, unbounded] - array of businessKey strings
  generic                : null, // [required, attribute]
}

/* encoder */
function Get_BusinessDetailExt() { }

Get_BusinessDetailExt.prototype = 
{
  businessKeys           : null, // [required, unbounded] - array of businessKey strings
  generic                : null, // [required, attribute]
}

/* encoder */
function Get_ServiceDetail() { }

Get_ServiceDetail.prototype = 
{
  serviceKeys            : null, // [required, unbounded] - array of serviceKey strings
  generic                : null, // [required, attribute]
}

/* encoder */
function Get_TModelDetail() { }

Get_TModelDetail.prototype = 
{
  tModelKeys             : null, // [required, unbounded] - array of tModelKey strings
  generic                : null, // [required, attribute]
}


// ----------------------------------------------------------------------------
// UDDI Inquiry Response Message types - alpha sort
// ----------------------------------------------------------------------------

/* decoder */
function BindingDetail() { }

BindingDetail.prototype = 
{
  bindingTemplates       : null, // [optional, unbounded] - array of BindingTemplate objects
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]

  toString : function () {
    return "BindingDetail[generic: " + this.generic + " operator: " + this.operator + " truncated: " + this.truncated + "]";
  }
};

/* decoder */
function BusinessDetail() { }

BusinessDetail.prototype = 
{
  businessEntities       : null, // [optional, unbounded] - array of BusinessEntity objects
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function BusinessDetailExt() { }

BusinessDetailExt.prototype =
{
  businessEntityExts     : null, // [required, unbounded] - array of BusinessEntityExt objects
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
}

/* decoder */
function BusinessList() { }

BusinessList.prototype = 
{
  businessInfos          : null, // [required, unbounded] - shortcut-array of BusinessInfo objects (can be empty)
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function RelatedBusinessesList() { }

RelatedBusinessesList.prototype = 
{
  businessKey            : null, // [required] - string
  relatedBusinessInfos   : null, // [required, unbounded] - shortcut-array of RelatedBusinessInfo objects (can be empty)
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function ServiceDetail() { }

ServiceDetail.prototype = 
{
  businessServices       : null, // [optional, unbounded] - array of BusinessService objects
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function ServiceList() { }

ServiceList.prototype = 
{
  serviceInfos           : null, // [required, unbounded] - shortcut-array of ServiceInfo objects (can be empty)
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function TModelDetail() { }

TModelDetail.prototype = 
{
  tModels                : null, // [required, unbounded] - array of TModel objects
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
};

/* decoder */
function TModelList() { }

TModelList.prototype =
{
  tModelInfos            : null, // [required, unbounded] - shortcut-array of TModelInfo objects (can be empty)
  generic                : null, // [required, attribute]
  operator               : null, // [required, attribute]
  truncated              : null, // [optional, attribute]
}



// ----------------------------------------------------------------------------
// UDDI Inquiry Registry Content types - alpha sort
// ----------------------------------------------------------------------------

// XXX make one last pass through the content type definitions to clean up
//     note the shortcut arrays and which ones can be empty!!

/* decoder */
function AccessPoint() { }

AccessPoint.prototype = 
{
  stringValue            : null, // [required] - string
  urlType                : null, // [required, attribute] - restricted to certain values (mailto, http, etc.)
};

/* decoder */
function Address() { }

Address.prototype = 
{
  addressLines           : null, // [optional] - array of AddressLine objects
  useType                : null, // [optional, attribute]
  sortCode               : null, // [optional, attribute]
  tModelKey              : null, // [optional, attribute]
};

/* decoder */
function AddressLine() { }

AddressLine.prototype = 
{
  stringValue            : null, // [required] - string
  keyName                : null, // [optional, attribute]
  KeyValue               : null, // [optional, attribute]
};

/* decoder */
function BindingTemplate() { }

BindingTemplate.prototype = 
{
  descriptions           : null, // [optional, unbounded] - array of Description objects
  accessPoint            : null, // [required, choice]
  hostingRedirector      : null, // [required, choice]
  tModelInstanceDetails  : null, // [required] - shortcut-array of TModelInstanceInfo objects (can be empty)
  serviceKey             : null, // [optional, attribute]
  bindingKey             : null, // [required, attribute]
};

/* bindingTemplates - just an array */

/* decoder */
function BusinessEntity() { }

BusinessEntity.prototype = 
{
  discoveryURLs          : null, // [optional] - shortcut-array of DiscoveryURL objects (can not be empty)
  names                  : null, // [required, unbounded] - array of Name objects
  descriptions           : null, // [optional, unbounded] - array of Description objects
  contacts               : null, // [optional] - shortcut - array of Contact objects (can not be empty)
  businessServices       : null, // [optional] - shortcut-array of BusinessService objects (can be empty)
  identifierBag          : null, // [optional]
  categoryBag            : null, // [optional]
  businessKey            : null, // [required, attribute]
  operator               : null, // [optional, attribute]
  authorizedName         : null, // [optional, attribute]
};

/* decoder */
function BusinessEntityExt() { }

BusinessEntityExt.prototype = 
{
  businessEntity         : null, // [required]
  extensions             : null, // [optional, unbounded] - array of XML elements
}

/* decoder */
function BusinessInfo() { }

BusinessInfo.prototype = 
{
  names              : null, // [required, unbounded] - array of Name Objects
  descriptions       : null, // [optional, unbounded] - array of Description objects
  serviceInfos       : null, // [required] - shortcut-array of ServiceInfo objects (can be empty)
  businessKey        : null, // [required, attribute]
};

/* businessInfos - just an array */

/* decoder */
function BusinessService() { }

BusinessService.prototype = 
{
  names                  : null, // [optional, unbounded] - array of Name objects
  descriptions           : null, // [optional, unbounded] - array of Description object
  bindingTemplates       : null, // [optional] - array of BindingTemplate objects (can be empty)
  categoryBag            : null, // [optional]
  serviceKey             : null, // [required, attribute]
  businessKey            : null, // [optional, attribute]
};

/* businessServices - just an array */

/* encoder, decoder*/
function CategoryBag() { }

/* encoder */
CategoryBag.prototype = 
{
  keyedReferences        : null, // [required, unbounded] - array of KeyedReference objects
};

/* decoder */
function Contact() { }

Contact.prototype = 
{
  descriptions           : null, // [optional, unbounded] - array of Description objects
  personName             : null, // [required] - string
  phones                 : null, // [optional, unbounded] - array of Phone objects
  emails                 : null, // [optional, unbounded] - array of email address strings
  addressses             : null, // [optional, unbounded] - array of Address objects
  useType                : null, // [optional, attribute]
};

/* contacts - just an array */

/* decoder */
function Description() { }

Description.prototype = 
{
  stringValue            : null, // [required]
  lang                   : null, // [optional, attribute]
};

/* encoder, decoder */
function DiscoveryURL() { }

DiscoveryURL.prototype = 
{
  stringValue            : null, // [required]
  useType                : null, // [required, attribute]
};

/* discoveryURLS - just an array */

/* decoder */
function Email() { }

Email.prototype = 
{
  stringValue            : null, // [required]
  useType                : null, // [optional, attribute]
}

/* findQualifier - just a string */

// XXX this can go, change to an array of strings in the owning object
/* encoder */
function FindQualifiers() { }

FindQualifiers.prototype = 
{
  findQualifiers         : null, // [optional, unbounded] - array of findQualifer strings
};

/* decoder */
function HostingRedirector() { }

HostingRedirector.prototype = 
{
  bindingKey             : null, // [required, attribute] - string
};

/* encoder, decoder */
function IdentifierBag() { }

IdentifierBag.prototype = 
{
  keyedReferences        : null, // [required, unbounded] - array of KeyedReference objects
};

/* decoder */
function InstanceDetails() { }

InstanceDetails.prototype = 
{
  descriptions           : null, // [optional, unbounded] - array of Description objects
  overviewDoc            : null, // [optional]
  instanceParms          : null, // [optional] - string
};

/* encoder, decoder */
function KeyedReference() { }

KeyedReference.prototype = 
{
  tModelKey              : null, // [optional, attribute] - string
  keyName                : null, // [optional, attribute] - string
  keyValue               : null, // [required, attribute] - string
};

/* encoder, decoder */
function Name() { }

Name.prototype = 
{
  stringValue            : null, // [required]
  lang                   : null, // [optional, attribute] - string
}

/* decoder */
function OverviewDoc() { }

OverviewDoc.prototype = 
{
  descriptions           : null, // [optional, unbounded] - array of Description objects
  overviewURL            : null, // [optional] - string
};

/* decoder */
function Phone() { }

Phone.prototype = 
{
  stringValue            : null, // [required]
  useType                : null, // [optional, attribute]
}

/* decoder */
function RelatedBusinessInfo() { }

RelatedBusinessInfo.prototype = 
{
  businessKey            : null, // [required] - string
  names                  : null, // [required, unbounded] - array of Name objects (*defnd above*)
  descriptions           : null, // [optional, unbounded] - array of Description objects (*defnd above*)
  sharedRelationships    : null, // [required, max=2] - array of SharedRelationships objects. XXXjg required?
};

/* relatedBusinessInfos - just an array */

/* decoder */
function ServiceInfo() { }

ServiceInfo.prototype = 
{
  names                  : null, // [optional, unbounded] - array of Name Objects
  businessKey            : null, // [required, attribute] - string
  serviceKey             : null, // [required, attribute] - string
};

/* serviceInfos - just an array */

/* decoder */
function SharedRelationships() { }

SharedRelationships.prototype = 
{
  keyedReferences        : null, // [required, unbounded] - array of KeyedReference objects
  direction              : null, // [required, attribute] - string, limited to [toKey | fromKey]
};

/* decoder */
function TModel() { }

TModel.prototype = 
{
  name                   : null, // [required]
  descriptions           : null, // [optional, unbounded]
  overviewDoc            : null, // [optional]
  identifierBag          : null, // [optional]
  categoryBag            : null, // [optional]
  tModelKey              : null, // [required, attribute]
  operator               : null, // [optional, attribute]
  authorizedName         : null, // [optional, attribute]
}

// XXX this can go, change to an array of tModelKey strings in the owning object
/* encoder */
function TModelBag() { }

TModelBag.prototype = 
{
  tModelKeys             : null, // [required, unbounded] - array of tModelKey strings
};

/* decoder */
function TModelInfo() { }

TModelInfo.prototype = 
{
  name                   : null, // [required]
  tModelKey              : null, // [required, attribute]
};

/* tModelInfos - just an array of tModelInfo objects */
/* tModelInstanceDetails - just an array of tModelInstanceInfo objects */

/* decoder */
function TModelInstanceInfo() { }

TModelInstanceInfo.prototype = 
{
  descriptions           : null, // [optional, unbounded] - array of Description objects
  instanceDetails        : null, // [optional]
  tModelKey              : null, // [required, attribute] - string
};

// ----------------------------------------------------------------------------
// END OF FILE
// ----------------------------------------------------------------------------


