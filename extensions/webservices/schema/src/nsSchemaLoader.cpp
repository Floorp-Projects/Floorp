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

#include "nsSchemaPrivate.h"
#include "nsSchemaLoader.h"

nsSchemaLoader::nsSchemaLoader()
{
  NS_INIT_ISUPPORTS();
}

nsSchemaLoader::~nsSchemaLoader()
{
}

NS_IMPL_ISUPPORTS1(nsSchemaLoader, nsISchemaLoader)

/* nsISchema load (in nsIURI schemaURI); */
NS_IMETHODIMP 
nsSchemaLoader::Load(nsIURI *schemaURI, nsISchema **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void loadAsync (in nsIURI schemaURI, in nsISchemaLoadListener listener); */
NS_IMETHODIMP 
nsSchemaLoader::LoadAsync(nsIURI *schemaURI, nsISchemaLoadListener *listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISchema processSchemaElement (in nsIDOMElement element); */
NS_IMETHODIMP 
nsSchemaLoader::ProcessSchemaElement(nsIDOMElement *element, 
                                     nsISchema **_retval)
{
  nsAutoString dummy;

  nsSchema* schema = new nsSchema(dummy);
  nsSchemaBuiltinType* bt = new nsSchemaBuiltinType(1);
  nsSchemaListType* lt = new nsSchemaListType(schema, dummy);
  nsSchemaUnionType* ut = new nsSchemaUnionType(schema, dummy);
  nsSchemaRestrictionType* rt = new nsSchemaRestrictionType(schema, dummy);
  nsSchemaComplexType* ct = new nsSchemaComplexType(schema, dummy, PR_FALSE);
  nsSchemaModelGroup* mg = new nsSchemaModelGroup(schema, dummy, 1);
  nsSchemaModelGroupRef* mgr = new nsSchemaModelGroupRef(schema, dummy);
  nsSchemaAnyParticle* ap = new nsSchemaAnyParticle(schema);
  nsSchemaElement* el = new nsSchemaElement(schema, dummy);
  nsSchemaElementRef* elr = new nsSchemaElementRef(schema, dummy);
  nsSchemaAttribute* att = new nsSchemaAttribute(schema, dummy);
  nsSchemaAttributeRef* attr = new nsSchemaAttributeRef(schema, dummy);
  nsSchemaAttributeGroup* attg = new nsSchemaAttributeGroup(schema, dummy);
  nsSchemaAttributeGroupRef* attgr = new nsSchemaAttributeGroupRef(schema, dummy);
  nsSchemaAnyAttribute* ant = new nsSchemaAnyAttribute(schema);
  nsSchemaFacet* fac = new nsSchemaFacet(schema, 1, PR_FALSE);

  return NS_OK;
}
