/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer (jband@netscape.com) (Original author)
 *   Vidur Apparao (vidur@netscape.com)
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

#include "wspprivate.h"

/***************************************************************************/
// IIDX is used as a way to hold and share our common set of interface
// indexes.

class IIDX {
public:
  enum IndexID {
    IDX_nsISupports,
    IDX_nsIException,
    IDX_nsIWebServiceCallContext,
    IDX_nsIVariant,
    IDX_nsIDOMElement,

    IDX_Count  // Just a count of the above.
  };

  PRUint16  Get(IndexID id) const
  {
    NS_ASSERTION(id < IDX_Count, "bad");
    return mData[id];
  }

  PRUint16* GetAddr(IndexID id)
  {
    NS_ASSERTION(id < IDX_Count, "bad");
    return &mData[id];
  }

private:
  PRUint16 mData[IDX_Count];
};

/***************************************************************************/
// ParamAccumulator helps us build param arrays without knowing the length
// ahead of time. We later memcpy out of the array into space allocated using
// nsIGenericInterfaceInfoSet::AllocateParamArray.

class ParamAccumulator
{
private:
  enum {
    MAX_BUILTIN = 8,
    ALLOCATION_INCREMENT = 16,
    MAX_TOTAL = 255
  }; // The typelib format limits us to 255 params.

public:
  PRUint16 GetCount() const
  {
    return mCount;
  }
  XPTParamDescriptor* GetArray()
  {
    return mArray;
  }
  void Clear()
  {
    mCount = 0;
  }

  XPTParamDescriptor* GetNextParam();

  ParamAccumulator()
    : mCount(0), mAvailable(MAX_BUILTIN), mArray(mBuiltinSpace)
  {
  }

  ~ParamAccumulator()
  {
    if(mArray != mBuiltinSpace)
      delete [] mArray;
  }
private:
  PRUint16            mCount;
  PRUint16            mAvailable;
  XPTParamDescriptor* mArray;
  XPTParamDescriptor  mBuiltinSpace[MAX_BUILTIN];
};

XPTParamDescriptor*
ParamAccumulator::GetNextParam()
{
  if (mCount == MAX_TOTAL) {
    NS_WARNING("Too many params!");
    return nsnull;
  }
  if (mCount == mAvailable) {
    PRUint16 newAvailable = mAvailable + ALLOCATION_INCREMENT;
    XPTParamDescriptor* newArray = new XPTParamDescriptor[newAvailable];
    if (!newArray) {
      return nsnull;
    }

    memcpy(newArray, mArray, newAvailable * sizeof(XPTParamDescriptor));

    if (mArray != mBuiltinSpace) {
      // The old array was heap allocated, delete so that we don't
      // leak it.
      delete [] mArray;
    }

    mArray = newArray;
    mAvailable = newAvailable;
  }

  XPTParamDescriptor* p = &mArray[mCount++];
  memset(p, 0, sizeof(XPTParamDescriptor));
  return p; 
}

/***************************************************************************/
// NewUniqueID generates a uuid that is intended to be locally unique for the
// life of the process. These uuids should not be shared with other processes
// or persisted. 

static nsresult
NewUniqueID(nsID *aID)
{
  // XXX Hack Alert. This was generated on jband's 'bugsfree' machine and
  // *ought* not to conflict with any existing guids. We should find a
  // more foolproof crossplatfor dynmic guidgen scheme.

  // {00000000-1063-11d6-98A8-00C04FA0D259}
  static const nsID sBaseGuid =
    {0x00000000, 0x1063, 0x11d6,
      {0x98, 0xa8, 0x0, 0xc0, 0x4f, 0xa0, 0xd2, 0x59}};

  static PRInt32 sSerialNumber = 0;

  *aID = sBaseGuid;
  aID->m0 = (PRUint32) PR_AtomicIncrement(&sSerialNumber);
  return NS_OK;
}

/***************************************************************************/
// FindInterfaceByName finds an interface info keyed by interface name. It
// searches the super manager and any additional managers

static nsresult
FindInterfaceByName(const char* aName, nsIInterfaceInfoSuperManager* iism,
                    nsIInterfaceInfoManager **aSet, nsIInterfaceInfo **_retval)
{
  NS_ENSURE_ARG_POINTER(aSet);
  if (NS_SUCCEEDED(iism->GetInfoForName(aName, _retval)) && *_retval) {
    NS_ADDREF(*aSet = iism);
    return NS_OK;
  }

  PRBool yes;
  nsCOMPtr<nsISimpleEnumerator> list;

  if (NS_SUCCEEDED(iism->HasAdditionalManagers(&yes)) && yes &&
      NS_SUCCEEDED(iism->EnumerateAdditionalManagers(getter_AddRefs(list))) &&
      list) {
    PRBool more;
    nsCOMPtr<nsIInterfaceInfoManager> current;

    while (NS_SUCCEEDED(list->HasMoreElements(&more)) && more &&
           NS_SUCCEEDED(list->GetNext(getter_AddRefs(current))) && current) {
      if (NS_SUCCEEDED(current->GetInfoForName(aName, _retval)) &&
          *_retval) {
        NS_ADDREF(*aSet = current.get());
        return NS_OK;
      }
    }
  }

  return NS_ERROR_NO_INTERFACE;
}

/***************************************************************************/
// FindInterfaceByName finds the *index* of an interface info in the given
// generic info set keyed by interface name. 

static nsresult
FindInterfaceIndexByName(const char* aName, nsIInterfaceInfoSuperManager* iism,
                         nsIGenericInterfaceInfoSet* aSet, PRUint16* aIndex)
{
  nsresult rv = aSet->IndexOfByName(aName, aIndex);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfo> info;
  nsCOMPtr<nsIInterfaceInfoManager> unused;
  rv = FindInterfaceByName(aName, iism, getter_AddRefs(unused), getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return aSet->AppendExternalInterface(info, aIndex);
}

/***************************************************************************/
// AppendStandardInterface is used to 'seed' the generic info set with a
// specific interface info that we expect to find in the super manager keyed 
// by interface id.
 
static nsresult
AppendStandardInterface(const nsIID& iid, nsIInterfaceInfoSuperManager* iism,
                        nsIGenericInterfaceInfoSet* set, PRUint16* aIndex)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceInfo> tempInfo;

  rv = iism->GetInfoForIID(&iid, getter_AddRefs(tempInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return set->AppendExternalInterface(tempInfo, aIndex);
}

/***************************************************************************/
// BuildInterfaceName is used to construct the name of an interface
// based on three AStrings.

static void
BuildInterfaceName(const nsAString& qualifier, const nsAString& name,
                   const nsAString& uri, nsACString& aCIdentifier)
{
  WSPFactory::XML2C(qualifier, aCIdentifier);

  nsCAutoString temp;
  WSPFactory::XML2C(name, temp);
  aCIdentifier.Append(temp);

  WSPFactory::XML2C(uri, temp);
  aCIdentifier.Append(temp);
}

/***************************************************************************/
// Forward declaration...

static nsresult
AppendMethodsForModelGroup(nsIInterfaceInfoSuperManager* iism,
                           nsIGenericInterfaceInfoSet* aSet,
                           nsISchemaModelGroup* aModelGroup,
                           const IIDX& iidx, XPTParamDescriptor* defaultResult,
                           nsIGenericInterfaceInfo* aInfo,
                           const nsAString& qualifier);

// Forward declaration...
static nsresult
GetParamDescOfType(nsIInterfaceInfoSuperManager* iism,
                   nsIGenericInterfaceInfoSet* aSet,
                   nsISchemaType* aType, const IIDX& iidx,
                   XPTParamDescriptor* defaultResult,
                   const nsAString& qualifier, PRUint32 depth,
                   ParamAccumulator* aParams);

/***************************************************************************/
// AppendMethodForParticle appends a method to a 'struct' interface 
// to represent the given nsISchemaParticle. It knows how to flatten
// particles that are themselves modelgroups (by recurring into 
// AppendMethodsForModelGroup). At also knows how to deal with arrays.

static nsresult
AppendMethodForParticle(nsIInterfaceInfoSuperManager* iism,
                        nsIGenericInterfaceInfoSet* aSet,
                        nsISchemaParticle* aParticle, const IIDX& iidx,
                        XPTParamDescriptor* defaultResult,
                        nsIGenericInterfaceInfo* aInfo,
                        const nsAString& qualifier)
{
  nsresult rv;
  XPTMethodDescriptor methodDesc;
  XPTParamDescriptor* pparamDesc;
  PRUint16 ignoredIndex;
  XPTParamDescriptor* paramArray;
  ParamAccumulator params;
  PRUint16 i;

  // If the particle is itself a modelGroup, then flatten in its methods.
  nsCOMPtr<nsISchemaModelGroup> modelGroup(do_QueryInterface(aParticle));
  if (modelGroup) {
    return AppendMethodsForModelGroup(iism, aSet, modelGroup, iidx, 
                                      defaultResult, aInfo, qualifier);
  }
  // else...

  nsCOMPtr<nsISchemaElement> schemaElement(do_QueryInterface(aParticle));
  if (!schemaElement) {
    // XXX we are considering this an error. (e.g is a nsISchemaAnyParticle).
    // XXX need better error tracking!
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISchemaType> schemaType;
  schemaElement->GetType(getter_AddRefs(schemaType));
  if (!schemaType) {
    // XXX need better error tracking!
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString name;
  rv = aParticle->GetName(name);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCAutoString identifierName;
  WSPFactory::XML2C(name, identifierName);

  rv = GetParamDescOfType(iism, aSet, schemaType, iidx, defaultResult,
                          qualifier, 0, &params);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aSet->AllocateParamArray(params.GetCount(), &paramArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  pparamDesc = params.GetArray();
  for (i = 0; i < params.GetCount(); pparamDesc++, i++) {
    // handle AString 'out' passing convensions
    pparamDesc->flags |=
      (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_DOMSTRING) ?
          (XPT_PD_IN | XPT_PD_DIPPER) : XPT_PD_OUT;

    // handle array size_of/length_of.
    if (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_ARRAY) {
      pparamDesc->type.argnum = 
          pparamDesc->type.argnum2 = i - 1; 
    }
    
    // handle trailing retval.
    if (i+1 == params.GetCount()) {
      pparamDesc->flags |= XPT_PD_RETVAL;
    }
  }

  memcpy(paramArray, params.GetArray(), 
         params.GetCount() * sizeof(XPTParamDescriptor));

  // XXX conditionally tack on 'Get' for array getter?
  // XXX Deal with intercaps in that case?

  methodDesc.flags    = params.GetCount() == 1 ? XPT_MD_GETTER : 0;
  methodDesc.name     = NS_CONST_CAST(char*, identifierName.get());
  methodDesc.params   = paramArray;
  methodDesc.result   = defaultResult;
  methodDesc.num_args = (PRUint8) params.GetCount();

  return aInfo->AppendMethod(&methodDesc, &ignoredIndex);
}

/***************************************************************************/
// AppendMethodsForModelGroup iterates the group's particles and calls 
// AppendMethodForParticle for each.

static nsresult
AppendMethodsForModelGroup(nsIInterfaceInfoSuperManager* iism,
                           nsIGenericInterfaceInfoSet* aSet,
                           nsISchemaModelGroup* aModelGroup,
                           const IIDX& iidx, XPTParamDescriptor* defaultResult,
                           nsIGenericInterfaceInfo* aInfo,
                           const nsAString& qualifier)
{
  nsresult rv;
  PRUint32 particleCount;
  rv = aModelGroup->GetParticleCount(&particleCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (PRUint32 i = 0; i < particleCount; i++) {
    nsCOMPtr<nsISchemaParticle> particle;
    rv = aModelGroup->GetParticle(i, getter_AddRefs(particle));
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = AppendMethodForParticle(iism, aSet, particle, iidx, defaultResult,
                                 aInfo, qualifier);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

/***************************************************************************/
// FindOrConstructInterface is used for 'struct' interfaces that represent
// compound data. Like the names says, it will find an existing info or
// create one. In our world these 'struct' interfaces are discovered by
// name: qualifier+typename+typetargetnamespace.

static nsresult
FindOrConstructInterface(nsIInterfaceInfoSuperManager* iism,
                         nsIGenericInterfaceInfoSet* aSet,
                         nsISchemaComplexType* aComplexType,
                         nsISchemaModelGroup* aModelGroup,
                         const IIDX& iidx, XPTParamDescriptor* defaultResult,
                         const nsAString& qualifier,
                         PRUint16* aTypeIndex)
{
  nsresult rv;
  nsCAutoString qualifiedName;
  nsAutoString name;
  nsAutoString ns;
  nsCOMPtr<nsIGenericInterfaceInfo> newInfo;
  nsID tempID;
  PRBool haveUniqueID = PR_FALSE;
  
  rv = aComplexType->GetName(name);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (name.IsEmpty()) {
    // Fabricate a unique name for anonymous type.
    // Bug 199555
    ::NewUniqueID(&tempID);
    nsXPIDLCString idStr;
    idStr += tempID.ToString();
    name.AssignWithConversion(idStr);
    haveUniqueID = PR_TRUE;
  }
  else {
   rv = aComplexType->GetTargetNamespace(ns);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  BuildInterfaceName(qualifier, name, ns, qualifiedName);

  // Does the interface already exist?

  rv = FindInterfaceIndexByName(qualifiedName.get(), iism, aSet, aTypeIndex);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  // Need to create the interface.

  if (!haveUniqueID)
    ::NewUniqueID(&tempID);
  rv = aSet->CreateAndAppendInterface(qualifiedName.get(), tempID,
                                      iidx.Get(IIDX::IDX_nsISupports),
                                      XPT_ID_SCRIPTABLE,
                                      getter_AddRefs(newInfo),
                                      aTypeIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return AppendMethodsForModelGroup(iism, aSet, aModelGroup, iidx, 
                                    defaultResult, newInfo, qualifier);
}

/***************************************************************************/
// GetParamDescOfType fills in a param descriptor for a given schematype.
// The descriptor is appended to the ParamAccumulator passed in. In array cases
// it may append more than one param. For compound data it may do significant
// work (via FindOrConstructInterface). It is used both for populating the
// params of the primary interface and of 'struct' interfaces.
// Note that the flags that control param passing (such as XPT_PD_IN, 
// XPT_PD_OUT, XPT_PD_DIPPER, and XPT_PD_RETVAL) are *not* set by this method.

static nsresult
GetParamDescOfType(nsIInterfaceInfoSuperManager* iism,
                   nsIGenericInterfaceInfoSet* aSet, nsISchemaType* aType,
                   const IIDX& iidx, XPTParamDescriptor* defaultResult,
                   const nsAString& qualifier, PRUint32 depth,
                   ParamAccumulator* aParams)
{
  XPTTypeDescriptor* additionalType;
  PRUint16 typeIndex;

  nsCOMPtr<nsISchemaSimpleType> simpleType;
  nsresult rv;

  XPTParamDescriptor* paramDesc = aParams->GetNextParam();
  if (!paramDesc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsISchemaComplexType> complexType(do_QueryInterface(aType));
  if (complexType) {
    PRUint16 contentModel;
    rv = complexType->GetContentModel(&contentModel);
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRBool isArray;
    rv = complexType->GetIsArray(&isArray);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (isArray) {
      // Punt by calling this array an nsIVariant.
      // Fix bug 202485
      paramDesc->type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
      paramDesc->type.type.iface = iidx.Get(IIDX::IDX_nsIVariant);
      return NS_OK;
    }

    switch(contentModel) {
      case nsISchemaComplexType::CONTENT_MODEL_SIMPLE:
        rv = complexType->GetSimpleBaseType(getter_AddRefs(simpleType));
        if (NS_FAILED(rv)) {
          return rv;
        }
        goto do_simple;
      case nsISchemaComplexType::CONTENT_MODEL_ELEMENT_ONLY:
      case nsISchemaComplexType::CONTENT_MODEL_MIXED:
        break;
      default:
        NS_ERROR("unexpected contentModel!");
      case nsISchemaComplexType::CONTENT_MODEL_EMPTY:
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsISchemaModelGroup> modelGroup;
    rv = complexType->GetModelGroup(getter_AddRefs(modelGroup));
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRUint16 compositor;
    rv = modelGroup->GetCompositor(&compositor);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (compositor == nsISchemaModelGroup::COMPOSITOR_CHOICE) {
      // CHOICE not supported
      return NS_ERROR_UNEXPECTED;
    }

    // XXX I *think* we can safely assume that we've already handled the case
    // where the type is an array.

    rv = FindOrConstructInterface(iism, aSet, complexType, modelGroup, iidx, 
                                  defaultResult, qualifier, &typeIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }

    paramDesc->type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
    paramDesc->type.type.iface = typeIndex;
    return NS_OK;
  }

  // If it is not complex it *must* be simple.

  simpleType = do_QueryInterface(aType, &rv);
do_simple:
  if (!simpleType) {
    return rv;
  }

  // We just ignore the restrictions on restrictionTypes and get at the
  // underlying simple type.

  nsCOMPtr<nsISchemaRestrictionType> restrictionType;
  while (nsnull != (restrictionType = do_QueryInterface(simpleType))) {
    rv = restrictionType->GetBaseType(getter_AddRefs(simpleType));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsISchemaBuiltinType> builtinType(do_QueryInterface(simpleType));
  if (builtinType) {
    PRUint16 typeID;
    rv = builtinType->GetBuiltinType(&typeID);
    if (NS_FAILED(rv)) {
      return rv;
    }

    switch(typeID) {
      case nsISchemaBuiltinType::BUILTIN_TYPE_ANYTYPE:
        paramDesc->type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
        paramDesc->type.type.iface = iidx.Get(IIDX::IDX_nsIVariant);
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_STRING:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING:
      case nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN:
        paramDesc->type.prefix.flags = TD_DOMSTRING | XPT_TDP_POINTER;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE:
        paramDesc->type.prefix.flags = TD_INT8;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE:
        paramDesc->type.prefix.flags = TD_UINT8;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY:
      case nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY:
        // XXX We are treating this as an array of TD_UINT8.

        rv = aSet->AllocateAdditionalType(&typeIndex, &additionalType);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Copy the element type into the referenced additional type.
        additionalType->prefix.flags = TD_UINT8;

        // Add the leading 'length' param.
        paramDesc->type.prefix.flags = TD_UINT32;
      
        // Alloc another param descriptor to hold the array info.
        paramDesc = aParams->GetNextParam();
        if (!paramDesc) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        // Set this *second* param as an array and return.
        paramDesc->type.prefix.flags = TD_ARRAY | XPT_TDP_POINTER;
        paramDesc->type.type.additional_type = typeIndex;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
        // XXX need a longInteger class?
        paramDesc->type.prefix.flags = TD_UINT64;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER:
        // XXX need a longInteger class?
        paramDesc->type.prefix.flags = TD_INT64;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_INT:
        paramDesc->type.prefix.flags = TD_INT32;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT:
        paramDesc->type.prefix.flags = TD_UINT32;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_LONG:
        paramDesc->type.prefix.flags = TD_INT64;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG:
        paramDesc->type.prefix.flags = TD_UINT64;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_SHORT:
        paramDesc->type.prefix.flags = TD_INT16;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT:
        paramDesc->type.prefix.flags = TD_UINT16;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL:
        // XXX need a decimal class?
        paramDesc->type.prefix.flags = TD_DOUBLE;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT:
        paramDesc->type.prefix.flags = TD_FLOAT;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE:
        paramDesc->type.prefix.flags = TD_DOUBLE;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN:
        paramDesc->type.prefix.flags = TD_BOOL;
        return NS_OK;
      case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME:
        // XXX date type?
      case nsISchemaBuiltinType::BUILTIN_TYPE_TIME:
      case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION:
      case nsISchemaBuiltinType::BUILTIN_TYPE_DATE:
      case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH:
      case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR:
      case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH:
      case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY:
      case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY:
        paramDesc->type.prefix.flags = TD_DOMSTRING | XPT_TDP_POINTER;
        return NS_OK;

      case nsISchemaBuiltinType::BUILTIN_TYPE_NAME:
      case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME:
      case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI:
      case nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE:
      case nsISchemaBuiltinType::BUILTIN_TYPE_ID:
      case nsISchemaBuiltinType::BUILTIN_TYPE_IDREF:
      case nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS:
      case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY:
      case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NOTATION:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN:
      case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS:
        paramDesc->type.prefix.flags = TD_DOMSTRING | XPT_TDP_POINTER;
        return NS_OK;

      default:
        NS_ERROR("unexpected typeID!");
        return NS_ERROR_UNEXPECTED;
    }
    NS_ERROR("missing return");
    return NS_ERROR_UNEXPECTED;
  }

  // else...

  nsCOMPtr<nsISchemaListType> listType(do_QueryInterface(simpleType));
  if (listType) {
    paramDesc->type.prefix.flags = TD_DOMSTRING | XPT_TDP_POINTER;
    return NS_OK;
  }

  // else...

  nsCOMPtr<nsISchemaUnionType> unionType(do_QueryInterface(simpleType));
  if (unionType) {
    paramDesc->type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
    paramDesc->type.type.iface = iidx.Get(IIDX::IDX_nsIVariant);
    return NS_OK;
  }

  // else...

  NS_ERROR("unexpected simple type!");
  return NS_ERROR_UNEXPECTED;
}

/***************************************************************************/
// GetParamDescOfPart is used in building the primary interface. It figures
// out if encoding of a given param is even necessary and then finds the
// appropriate schema type so that it can call GetParamDescOfType to do the
// rest of the work of filliung in a param descriptor.

static nsresult
GetParamDescOfPart(nsIInterfaceInfoSuperManager* iism,
                   nsIGenericInterfaceInfoSet* aSet, nsIWSDLPart* aPart,
                   const IIDX& iidx, XPTParamDescriptor* defaultResult,
                   const nsAString& qualifier, ParamAccumulator* aParams)
{
  nsresult rv;

  // If the binding is 'literal' then we consider this as DOM element.
  nsCOMPtr<nsIWSDLBinding> binding;
  rv = aPart->GetBinding(getter_AddRefs(binding));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISOAPPartBinding> soapPartBinding(do_QueryInterface(binding));
  if (soapPartBinding) {
    PRUint16 use;
    rv = soapPartBinding->GetUse(&use);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (use == nsISOAPPartBinding::USE_LITERAL) {
      XPTParamDescriptor* paramDesc = aParams->GetNextParam();
      if (!paramDesc) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      paramDesc->type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
      paramDesc->type.type.iface = iidx.Get(IIDX::IDX_nsIDOMElement);
      return NS_OK;
    }
  }

  nsCOMPtr<nsISchemaComponent> schemaComponent;
  rv = aPart->GetSchemaComponent(getter_AddRefs(schemaComponent));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISchemaType> type;
  nsCOMPtr<nsISchemaElement> element(do_QueryInterface(schemaComponent));
  if (element) {
    rv = element->GetType(getter_AddRefs(type));
  }
  else {
    type = do_QueryInterface(schemaComponent, &rv);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetParamDescOfType(iism, aSet, type, iidx, defaultResult, qualifier,
                            0, aParams);
}

/***************************************************************************/
// AccumulateParamsForMessage iterates the parts of a given message and
// accumulates the param descriptors.

static nsresult
AccumulateParamsForMessage(nsIInterfaceInfoSuperManager* iism,
                           nsIGenericInterfaceInfoSet* aSet,
                           nsIWSDLMessage* aMsg, const IIDX& iidx,
                           XPTParamDescriptor* defaultResult,
                           const nsAString& qualifier,
                           ParamAccumulator* aParams)
{
  nsresult rv;
  PRUint32 partCount;
  
  rv = aMsg->GetPartCount(&partCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (PRUint32 i = 0; i < partCount; i++) {
    nsCOMPtr<nsIWSDLPart> part;
    rv = aMsg->GetPart(i, getter_AddRefs(part));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Accumulate paramDescriptors (except some flags). This might include
    // constructing a nested set of compound types and adding them
    // to the set.

    rv = GetParamDescOfPart(iism, aSet, part, iidx, defaultResult, qualifier,
                            aParams);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS1(nsWSPInterfaceInfoService, nsIWSPInterfaceInfoService)

nsWSPInterfaceInfoService::nsWSPInterfaceInfoService()
{
}

nsWSPInterfaceInfoService::~nsWSPInterfaceInfoService()
{
  // empty
}

/***************************************************************************/
// InfoForPort takes a nsIWSDLPort, qualifier name, and isAsync flag and
// finds or constructs the primary interface info for that port. It also
// finds or constructs all the interface infos used in params to its methods.
// It returns the interface info and optionally the interface info set - 
// which allows the caller to gather information on the referenced interfaces. 

/* nsIInterfaceInfo infoForPort (in nsIWSDLPort aPort, in AString
   aPortURL, in AString aQualifier, in PRBool aIsAsync, out
   nsIInterfaceInfoManager aSet); */
NS_IMETHODIMP
nsWSPInterfaceInfoService::InfoForPort(nsIWSDLPort *aPort,
                                       const nsAString & aPortURL,
                                       const nsAString & aQualifier,
                                       PRBool aIsAsync,
                                       nsIInterfaceInfoManager **aSet,
                                       nsIInterfaceInfo **_retval)
{
  if (!aPort) {
    // aPort can't be null.
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  nsCAutoString primaryName;
  nsCAutoString primaryAsyncName;
  nsAutoString portName;

  nsCOMPtr<nsIInterfaceInfoSuperManager> iism =
    do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID);
  if (!iism) {
    return NS_ERROR_UNEXPECTED;
  }

  // Build the primary and primaryAsync interface names.

  rv = aPort->GetName(portName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  BuildInterfaceName(aQualifier, portName, aPortURL, primaryName);
  primaryAsyncName.Assign(primaryName);
  primaryAsyncName.Append("Async");

  // With luck the work has already been done and we can just return the
  // existing info.

  if (NS_SUCCEEDED(FindInterfaceByName(aIsAsync ? primaryAsyncName.get() :
                                                  primaryName.get(),
                                       iism, aSet, _retval))) {
    return NS_OK;
  }

  // No, we need to build all the stuff...

  nsCOMPtr<nsIGenericInterfaceInfo> primaryInfo;
  nsCOMPtr<nsIGenericInterfaceInfo> primaryAsyncInfo;
  nsCOMPtr<nsIGenericInterfaceInfo> listenerInfo;

  ParamAccumulator inParams;
  ParamAccumulator outParams;

  XPTParamDescriptor* tempParamArray;
  XPTParamDescriptor* defaultResult;

  XPTParamDescriptor* primaryParamArray;
  XPTParamDescriptor* primaryAsyncParamArray;
  XPTParamDescriptor* listenerParamArray;

  nsCOMPtr<nsIInterfaceInfo> tempInfo;
  nsCAutoString tempCString;
  nsAutoString tempString;

  IIDX iidx;
  PRUint16 listenerIndex;

  XPTMethodDescriptor methodDesc;
  XPTParamDescriptor paramDesc;
  XPTParamDescriptor* pparamDesc;

  // Create a new info set.

  nsCOMPtr<nsIGenericInterfaceInfoSet> set =
    do_CreateInstance(NS_GENERIC_INTERFACE_INFO_SET_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Seed the info set with commonly needed interfaces.

  rv = AppendStandardInterface(NS_GET_IID(nsISupports), iism, set,
                               iidx.GetAddr(IIDX::IDX_nsISupports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AppendStandardInterface(NS_GET_IID(nsIException), iism, set,
                               iidx.GetAddr(IIDX::IDX_nsIException));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AppendStandardInterface(NS_GET_IID(nsIWebServiceCallContext), iism, set,
                               iidx.GetAddr(IIDX::IDX_nsIWebServiceCallContext));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AppendStandardInterface(NS_GET_IID(nsIVariant), iism, set,
                               iidx.GetAddr(IIDX::IDX_nsIVariant));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = AppendStandardInterface(NS_GET_IID(nsIDOMElement), iism, set,
                               iidx.GetAddr(IIDX::IDX_nsIDOMElement));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create and add the primary interface.

  PRUint16 ignoredIndex;
  nsID tempID;

  ::NewUniqueID(&tempID);
  rv = set->CreateAndAppendInterface(primaryName.get(), tempID,
                                     iidx.Get(IIDX::IDX_nsISupports),
                                     XPT_ID_SCRIPTABLE,
                                     getter_AddRefs(primaryInfo),
                                     &ignoredIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create and add the primary async interface.

  ::NewUniqueID(&tempID);
  rv = set->CreateAndAppendInterface(primaryAsyncName.get(), tempID,
                                     iidx.Get(IIDX::IDX_nsISupports),
                                     XPT_ID_SCRIPTABLE,
                                     getter_AddRefs(primaryAsyncInfo),
                                     &ignoredIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }


  // Allocate the param info for the 'nsresult' return type that we reuse.

  rv = set->AllocateParamArray(1, &defaultResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  defaultResult->type.prefix.flags = TD_UINT32;
  defaultResult->flags = XPT_PD_OUT;

  // Create and add the listener interface.

  tempCString = primaryName;
  tempCString.Append("Listener");
  ::NewUniqueID(&tempID);
  rv = set->CreateAndAppendInterface(tempCString.get(), tempID,
                                     iidx.Get(IIDX::IDX_nsISupports),
                                     XPT_ID_SCRIPTABLE,
                                     getter_AddRefs(listenerInfo),
                                     &listenerIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Add the setListener method to the primaryAsync interface.
  // void setListener(in 'OurType'Listener listener);

  rv = set->AllocateParamArray(1, &tempParamArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  tempParamArray[0].type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
  tempParamArray[0].type.type.iface = listenerIndex;
  tempParamArray[0].flags = XPT_PD_IN;
  methodDesc.name     = "setListener";
  methodDesc.params   = tempParamArray;
  methodDesc.result   = defaultResult;
  methodDesc.flags    = 0;
  methodDesc.num_args = 1;

  rv = primaryAsyncInfo->AppendMethod(&methodDesc, &ignoredIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Add the onError method to the listener interface.
  // void onError(in nsIException error, in nsIWebServiceCallContext cx);

  rv = set->AllocateParamArray(2, &tempParamArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  tempParamArray[0].type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
  tempParamArray[0].type.type.iface = iidx.Get(IIDX::IDX_nsIException);
  tempParamArray[0].flags = XPT_PD_IN;

  tempParamArray[1].type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
  tempParamArray[1].type.type.iface =
    iidx.Get(IIDX::IDX_nsIWebServiceCallContext);
  tempParamArray[1].flags = XPT_PD_IN;

  methodDesc.name     = "onError";
  methodDesc.params   = tempParamArray;
  methodDesc.result   = defaultResult;
  methodDesc.flags    = 0;
  methodDesc.num_args = 2;

  rv = listenerInfo->AppendMethod(&methodDesc, &ignoredIndex);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Add the methods.

  PRUint32 methodCount;
  rv = aPort->GetOperationCount(&methodCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 i;
  for (i = 0; i < methodCount; i++)
  {
    nsCOMPtr<nsIWSDLOperation> op;
    rv = aPort->GetOperation(i, getter_AddRefs(op));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Accumulate the input params.

    nsCOMPtr<nsIWSDLMessage> msg;
    rv = op->GetInput(getter_AddRefs(msg));
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    inParams.Clear();
    rv = AccumulateParamsForMessage(iism, set, msg, iidx, defaultResult, 
                                    aQualifier, &inParams);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Accumulate the output params.

    rv = op->GetOutput(getter_AddRefs(msg));
    if (NS_FAILED(rv)) {
      return rv;
    }

    outParams.Clear();
    rv = AccumulateParamsForMessage(iism, set, msg, iidx, defaultResult, 
                                    aQualifier, &outParams);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // XXX do we need types for the faults?
    // XXX ignoring param ordering problem for now.

    // Allocate the param arrays. On the other hand
    // no need to allocate the param arrays if the
    // input type and the output type are null; fixing
    // bug 200767
    PRUint32 k = 0;
    PRUint16 primaryParamCount = inParams.GetCount() + outParams.GetCount();
    if (primaryParamCount != 0) { 
      rv = set->AllocateParamArray(primaryParamCount, &primaryParamArray);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Set the appropriate 'in' param flags.
    
      // 'input' param cases are easy because they are exactly the same for
      // primary and primaryAsync.

      pparamDesc = inParams.GetArray();

      for (k = 0; k < inParams.GetCount(); pparamDesc++, k++) {
        // set direction flag
        pparamDesc->flags |= XPT_PD_IN;

        // handle array size_of/length_of.
        if (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_ARRAY) {
          pparamDesc->type.argnum = 
              pparamDesc->type.argnum2 = k - 1; 
        }
      }
        
      // Copy the 'in' param arrays.

      memcpy(primaryParamArray, inParams.GetArray(), 
             inParams.GetCount() * sizeof(XPTParamDescriptor));
    
      // Do 'output' param cases for primary interface.

      pparamDesc = outParams.GetArray();
      for (k = 0; k < outParams.GetCount(); pparamDesc++, k++) {
        // handle AString 'out' passing convensions
        pparamDesc->flags |=
          (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_DOMSTRING) ?
              (XPT_PD_IN | XPT_PD_DIPPER) : XPT_PD_OUT;

        // handle array size_of/length_of.
        if (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_ARRAY) {
          pparamDesc->type.argnum =
              pparamDesc->type.argnum2 = inParams.GetCount() + k - 1; 
        }
      
        // handle trailing retval.
        if (k+1 == outParams.GetCount()) {
          pparamDesc->flags |= XPT_PD_RETVAL;
        }
      }

      memcpy(primaryParamArray + inParams.GetCount(), 
             outParams.GetArray(), 
             outParams.GetCount() * sizeof(XPTParamDescriptor));
      // primaryParamArray is done now.
    }


    PRUint16 primaryAsyncParamCount = inParams.GetCount() + 1;
    rv = set->AllocateParamArray(primaryAsyncParamCount,
                                 &primaryAsyncParamArray);
    if (NS_FAILED(rv)) {
      return rv;
    }

    memcpy(primaryAsyncParamArray, inParams.GetArray(), 
           inParams.GetCount() * sizeof(XPTParamDescriptor));


    // Add the trailing [retval] param for the Async call

    paramDesc.type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
    paramDesc.type.type.iface = iidx.Get(IIDX::IDX_nsIWebServiceCallContext);
    paramDesc.flags = XPT_PD_OUT | XPT_PD_RETVAL;
    primaryAsyncParamArray[inParams.GetCount()] = paramDesc;
    // primaryAsyncParamArray is done now.

    PRUint16 listenerParamCount = 1 + outParams.GetCount();
    rv = set->AllocateParamArray(listenerParamCount, &listenerParamArray);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Do 'output' param cases for listener interface.

    pparamDesc = outParams.GetArray();
    for (k = 0; k < outParams.GetCount(); pparamDesc++, k++) {
      // set direction flag
      pparamDesc->flags &= ~(XPT_PD_OUT | XPT_PD_DIPPER | XPT_PD_RETVAL);
      pparamDesc->flags |= XPT_PD_IN;
    
      // handle array size_of/length_of.
      if (XPT_TDP_TAG(pparamDesc->type.prefix) == TD_ARRAY) {
        pparamDesc->type.argnum = 
            pparamDesc->type.argnum2 = k - 1;  
      }
    }

    memcpy(listenerParamArray, 
           outParams.GetArray(), 
           outParams.GetCount() * sizeof(XPTParamDescriptor));

    // Add the trailing 'in nsIWebServiceCallContext cx' param for listener

    paramDesc.type.prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
    paramDesc.type.type.iface = iidx.Get(IIDX::IDX_nsIWebServiceCallContext);
    paramDesc.flags = XPT_PD_IN;
    listenerParamArray[listenerParamCount-1] = paramDesc;
    // listenerParamArray is done now.

    rv = op->GetName(tempString);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Append the methods...

    WSPFactory::XML2C(tempString, tempCString);

    methodDesc.name     = NS_CONST_CAST(char*, tempCString.get());
    methodDesc.params   = primaryParamArray;
    methodDesc.result   = defaultResult;
    methodDesc.flags    = 0;
    methodDesc.num_args = (PRUint8) primaryParamCount;

    rv = primaryInfo->AppendMethod(&methodDesc, &ignoredIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }


    methodDesc.name     = NS_CONST_CAST(char*, tempCString.get());
    methodDesc.params   = primaryAsyncParamArray;
    methodDesc.result   = defaultResult;
    methodDesc.flags    = 0;
    methodDesc.num_args = (PRUint8) primaryAsyncParamCount;

    rv = primaryAsyncInfo->AppendMethod(&methodDesc, &ignoredIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }


    tempCString.Append("Callback");
    methodDesc.name     = NS_CONST_CAST(char*, tempCString.get());
    methodDesc.params   = listenerParamArray;
    methodDesc.result   = defaultResult;
    methodDesc.flags    = 0;
    methodDesc.num_args = (PRUint8) listenerParamCount;

    rv = listenerInfo->AppendMethod(&methodDesc, &ignoredIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // 'Publish' our new set by adding it to the global additional managers list.

  rv = iism->AddAdditionalManager(set);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Optionally return our new set.

  if (aSet) {
    NS_ADDREF(*aSet = set);
  }

  // Return the appropriate interface info.

  if (aIsAsync) {
    NS_ADDREF(*_retval = primaryAsyncInfo);
  }
  else {
    NS_ADDREF(*_retval = primaryInfo);
  }

  return NS_OK;
}

