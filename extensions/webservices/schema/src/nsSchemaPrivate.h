/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#ifndef __nsSchemaPrivate_h__
#define __nsSchemaPrivate_h__

#include "nsISchema.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsSupportsArray.h"
#include "nsHashtable.h"
#include "nsString.h"

#define NS_SCHEMA_NAMESPACE "http://www.w3.org/2001/XMLSchema"

class nsSchema : public nsISchema 
{
public:
  nsSchema(const nsAReadableString& aTargetNamespace);
  virtual ~nsSchema();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMACOMPONENT
  NS_DECL_NSISCHEMA

  NS_IMETHOD AddType(nsISchemaType* aType);
  NS_IMETHOD AddAttribute(nsISchemaAttribute* aAttribute);
  NS_IMETHOD AddElement(nsISchemaElement* aElement);
  NS_IMETHOD AddAttributeGroup(nsISchemaAttributeGroup* aAttributeGroup);
  NS_IMETHOD AddModelGroup(nsISchemaModelGroup* aModelGroup);
  nsresult ResolveTypePlaceholder(nsISchemaType* aPlaceholder,
                                  nsISchemaType** aType);

protected:
  nsString mTargetNamespace;
  nsSupportsArray mTypes;
  nsSupportsHashtable mTypesHash;
  nsSupportsArray mAttributes;
  nsSupportsHashtable mAttributesHash;
  nsSupportsArray mElements;
  nsSupportsHashtable mElementsHash;
  nsSupportsArray mAttributeGroups;
  nsSupportsHashtable mAttributeGroupsHash;
  nsSupportsArray mModelGroups;
  nsSupportsHashtable mModelGroupsHash;
};

class nsSchemaComponentBase {
public:
  nsSchemaComponentBase(nsSchema* aSchema);
  virtual ~nsSchemaComponentBase();

  NS_IMETHOD GetTargetNamespace(nsAWritableString& aTargetNamespace);

protected:
  nsSchema* mSchema;  // [WEAK] It owns me
  // Used to prevent infinite recursion for cycles in the object graph
  PRPackedBool mIsResolving;
  PRPackedBool mIsClearing;
};

#define NS_IMPL_NSISCHEMACOMPONENT_USING_BASE                           \
  NS_IMETHOD GetTargetNamespace(nsAWritableString& aTargetNamespace) {  \
    return nsSchemaComponentBase::GetTargetNamespace(aTargetNamespace); \
  }                                                                     \
  NS_IMETHOD Resolve();                                                 \
  NS_IMETHOD Clear();

class nsSchemaBuiltinType : public nsISchemaBuiltinType
{
public:
  nsSchemaBuiltinType(PRUint16 aBuiltinType);
  virtual ~nsSchemaBuiltinType();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMACOMPONENT
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMASIMPLETYPE
  NS_DECL_NSISCHEMABUILTINTYPE

protected:
  PRUint16 mBuiltinType;
};

class nsSchemaListType : public nsSchemaComponentBase,
                         public nsISchemaListType
{
public:
  nsSchemaListType(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaListType();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMASIMPLETYPE
  NS_DECL_NSISCHEMALISTTYPE
  
  NS_IMETHOD SetListType(nsISchemaSimpleType* aListType);

protected:
  nsString mName;
  nsCOMPtr<nsISchemaSimpleType> mListType;
};

class nsSchemaUnionType : public nsSchemaComponentBase,
                          public nsISchemaUnionType
{
public:
  nsSchemaUnionType(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaUnionType();
  
  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMASIMPLETYPE
  NS_DECL_NSISCHEMAUNIONTYPE

  NS_IMETHOD AddUnionType(nsISchemaSimpleType* aUnionType);

protected:
  nsString mName;
  nsSupportsArray mUnionTypes;
};

class nsSchemaRestrictionType : public nsSchemaComponentBase,
                                public nsISchemaRestrictionType
{
public:
  nsSchemaRestrictionType(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaRestrictionType();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMASIMPLETYPE
  NS_DECL_NSISCHEMARESTRICTIONTYPE

  NS_IMETHOD SetBaseType(nsISchemaSimpleType* aBaseType);
  NS_IMETHOD AddFacet(nsISchemaFacet* aFacet);

protected:
  nsString mName;
  nsCOMPtr<nsISchemaSimpleType> mBaseType;
  nsSupportsArray mFacets;
};

class nsSchemaComplexType : public nsSchemaComponentBase,
                            public nsISchemaComplexType
{
public:
  nsSchemaComplexType(nsSchema* aSchema, const nsAReadableString& aName,
                      PRBool aAbstract);
  virtual ~nsSchemaComplexType();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMACOMPLEXTYPE

  NS_IMETHOD SetContentModel(PRUint16 aContentModel);
  NS_IMETHOD SetDerivation(PRUint16 aDerivation, nsISchemaType* aBaseType);
  NS_IMETHOD SetSimpleBaseType(nsISchemaSimpleType* aSimpleBaseType);
  NS_IMETHOD SetModelGroup(nsISchemaModelGroup* aModelGroup);
  NS_IMETHOD AddAttribute(nsISchemaAttributeComponent* aAttribute);
  
protected:
  nsString mName;
  PRUint16 mContentModel;
  PRUint16 mDerivation;
  nsCOMPtr<nsISchemaType> mBaseType;
  nsCOMPtr<nsISchemaSimpleType> mSimpleBaseType;
  nsCOMPtr<nsISchemaModelGroup> mModelGroup;
  nsSupportsArray mAttributes;
  nsSupportsHashtable mAttributesHash;
  PRPackedBool mAbstract;
};

class nsSchemaTypePlaceholder : public nsSchemaComponentBase,
                                public nsISchemaSimpleType
{
public:
  nsSchemaTypePlaceholder(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaTypePlaceholder();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMATYPE
  NS_DECL_NSISCHEMASIMPLETYPE

protected:
  nsString mName;
};

class nsSchemaParticleBase : public nsSchemaComponentBase
{
public:  
  nsSchemaParticleBase(nsSchema* aSchema);
  virtual ~nsSchemaParticleBase();

  NS_IMETHOD GetMinOccurs(PRUint32 *aMinOccurs);
  NS_IMETHOD GetMaxOccurs(PRUint32 *aMaxOccurs);

  NS_IMETHOD SetMinOccurs(PRUint32 aMinOccurs);
  NS_IMETHOD SetMaxOccurs(PRUint32 aMaxOccurs);

protected:
  PRUint32 mMinOccurs;
  PRUint32 mMaxOccurs;
};

#define NS_IMPL_NSISCHEMAPARTICLE_USING_BASE                           \
  NS_IMETHOD GetMinOccurs(PRUint32 *aMinOccurs) {                      \
    return nsSchemaParticleBase::GetMinOccurs(aMinOccurs);             \
  }                                                                    \
  NS_IMETHOD GetMaxOccurs(PRUint32 *aMaxOccurs) {                      \
    return nsSchemaParticleBase::GetMaxOccurs(aMaxOccurs);             \
  }                                                                    \
  NS_IMETHOD SetMinOccurs(PRUint32 aMinOccurs) {                       \
    return nsSchemaParticleBase::SetMinOccurs(aMinOccurs);             \
  }                                                                    \
  NS_IMETHOD SetMaxOccurs(PRUint32 aMaxOccurs) {                       \
    return nsSchemaParticleBase::SetMaxOccurs(aMaxOccurs);             \
  }                                                                    \
  NS_IMETHOD GetParticleType(PRUint16 *aParticleType);                 \
  NS_IMETHOD GetName(nsAWritableString& aName);                        

class nsSchemaModelGroup : public nsSchemaParticleBase,
                           public nsISchemaModelGroup
{
public:
  nsSchemaModelGroup(nsSchema* aSchema, 
                     const nsAReadableString& aName,
                     PRUint16 aCompositor);
  virtual ~nsSchemaModelGroup();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_IMPL_NSISCHEMAPARTICLE_USING_BASE
  NS_DECL_NSISCHEMAMODELGROUP

  NS_IMETHOD AddParticle(nsISchemaParticle* aParticle);

protected:
  nsString mName;
  PRUint16 mCompositor;
  nsSupportsArray mParticles;
};

class nsSchemaModelGroupRef : public nsSchemaParticleBase,
                              public nsISchemaModelGroup
{
public:
  nsSchemaModelGroupRef(nsSchema* aSchema, 
                        const nsAReadableString& aRef);
  virtual ~nsSchemaModelGroupRef();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_IMPL_NSISCHEMAPARTICLE_USING_BASE
  NS_DECL_NSISCHEMAMODELGROUP

protected:
  nsString mRef;
  nsCOMPtr<nsISchemaModelGroup> mModelGroup;
};

class nsSchemaAnyParticle : public nsSchemaParticleBase,
                            public nsISchemaAnyParticle
{
public:
  nsSchemaAnyParticle(nsSchema* aSchema);
  virtual ~nsSchemaAnyParticle();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_IMPL_NSISCHEMAPARTICLE_USING_BASE
  NS_DECL_NSISCHEMAANYPARTICLE

  NS_IMETHOD SetProcess(PRUint16 aProcess);
  NS_IMETHOD SetNamespace(const nsAReadableString& aNamespace);

protected:
  PRUint16 mProcess;
  nsString mNamespace;
};

class nsSchemaElement : public nsSchemaParticleBase,
                        public nsISchemaElement
{
public:
  nsSchemaElement(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaElement();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_IMPL_NSISCHEMAPARTICLE_USING_BASE
  NS_DECL_NSISCHEMAELEMENT

  NS_IMETHOD SetType(nsISchemaType* aType);
  NS_IMETHOD SetConstraints(const nsAReadableString& aDefaultValue,
                            const nsAReadableString& aFixedValue);
  NS_IMETHOD SetFlags(PRBool aNillable, PRBool aAbstract);

protected:
  nsString mName;
  nsCOMPtr<nsISchemaType> mType;
  nsString mDefaultValue;
  nsString mFixedValue;
  PRPackedBool mNillable;
  PRPackedBool mAbstract;
};

class nsSchemaElementRef : public nsSchemaParticleBase,
                           public nsISchemaElement
{
public:
  nsSchemaElementRef(nsSchema* aSchema, const nsAReadableString& aRef);
  virtual ~nsSchemaElementRef();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_IMPL_NSISCHEMAPARTICLE_USING_BASE
  NS_DECL_NSISCHEMAELEMENT

protected:
  nsString mRef;
  nsCOMPtr<nsISchemaElement> mElement;
};

class nsSchemaAttribute : public nsSchemaComponentBase,
                          public nsISchemaAttribute 
{
public:
  nsSchemaAttribute(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaAttribute();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAATTRIBUTECOMPONENT
  NS_DECL_NSISCHEMAATTRIBUTE

  NS_IMETHOD SetType(nsISchemaSimpleType* aType);
  NS_IMETHOD SetConstraints(const nsAReadableString& aDefaultValue,
                            const nsAReadableString& aFixedValue);
  NS_IMETHOD SetUse(PRUint16 aUse);

protected:
  nsString mName;
  nsCOMPtr<nsISchemaSimpleType> mType;
  nsString mDefaultValue;
  nsString mFixedValue;
  PRUint16 mUse;
};

class nsSchemaAttributeRef : public nsSchemaComponentBase,
                             public nsISchemaAttribute 
{
public:
  nsSchemaAttributeRef(nsSchema* aSchema, const nsAReadableString& aRef);
  virtual ~nsSchemaAttributeRef();
  
  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAATTRIBUTECOMPONENT
  NS_DECL_NSISCHEMAATTRIBUTE

  NS_IMETHOD SetConstraints(const nsAReadableString& aDefaultValue,
                            const nsAReadableString& aFixedValue);
  NS_IMETHOD SetUse(PRUint16 aUse);

protected:
  nsString mRef;
  nsCOMPtr<nsISchemaAttribute> mAttribute;
  nsString mDefaultValue;
  nsString mFixedValue;
  PRUint16 mUse;
};

class nsSchemaAttributeGroup : public nsSchemaComponentBase,
                               public nsISchemaAttributeGroup
{
public:
  nsSchemaAttributeGroup(nsSchema* aSchema, const nsAReadableString& aName);
  virtual ~nsSchemaAttributeGroup();
  
  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAATTRIBUTECOMPONENT
  NS_DECL_NSISCHEMAATTRIBUTEGROUP
  
  NS_IMETHOD AddAttribute(nsISchemaAttributeComponent* aAttribute);

protected:
  nsString mName;
  nsSupportsArray mAttributes;
  nsSupportsHashtable mAttributesHash;
};

class nsSchemaAttributeGroupRef : public nsSchemaComponentBase,
                                  public nsISchemaAttributeGroup
{
public:
  nsSchemaAttributeGroupRef(nsSchema* aSchema, const nsAReadableString& aRef);
  virtual ~nsSchemaAttributeGroupRef();
  
  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAATTRIBUTECOMPONENT
  NS_DECL_NSISCHEMAATTRIBUTEGROUP

protected:
  nsString mRef;
  nsCOMPtr<nsISchemaAttributeGroup> mAttributeGroup;
};

class nsSchemaAnyAttribute : public nsSchemaComponentBase,
                             public nsISchemaAnyAttribute
{
public:
  nsSchemaAnyAttribute(nsSchema* aSchema);
  virtual ~nsSchemaAnyAttribute();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAATTRIBUTECOMPONENT
  NS_DECL_NSISCHEMAANYATTRIBUTE
  
  NS_IMETHOD SetProcess(PRUint16 aProcess);
  NS_IMETHOD SetNamespace(const nsAReadableString& aNamespace);

protected:
  PRUint16 mProcess;
  nsString mNamespace;
};

class nsSchemaFacet : public nsSchemaComponentBase,
                      public nsISchemaFacet
{
public:
  nsSchemaFacet(nsSchema* aSchema, PRUint16 aFacetType, PRBool aIsFixed);
  virtual ~nsSchemaFacet();

  NS_DECL_ISUPPORTS
  NS_IMPL_NSISCHEMACOMPONENT_USING_BASE
  NS_DECL_NSISCHEMAFACET

  NS_IMETHOD SetValue(const nsAReadableString& aStrValue);
  NS_IMETHOD SetDigitsValue(PRUint32 aDigitsValue);
  NS_IMETHOD SetWhitespaceValue(PRUint16 aWhitespaceValue);

protected:
  PRUint16 mFacetType;
  PRPackedBool mIsFixed;
  nsString mStrValue;
  PRUint32 mDigitsValue;
  PRUint16 mWhitespaceValue;
};

#endif // __nsSchemaPrivate_h__


