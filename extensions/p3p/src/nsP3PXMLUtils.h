/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PXMLUtils_h__
#define nsP3PXMLUtils_h__

#include <nsCom.h>

#include <nsIDOMNode.h>

#include <nsVoidArray.h>
#include <nsString.h>


class nsP3PXMLUtils {
public:
  static
  NS_METHOD             FindTag( const char  *aName,
                                 const char  *aNameSpace,
                                 nsIDOMNode  *aDOMNode,
                                 nsIDOMNode **aDOMNodeWithTag );
  static
  NS_METHOD             FindTag( nsStringArray  *aName,
                                 const char     *aNameSpace,
                                 nsIDOMNode     *aDOMNode,
                                 nsIDOMNode    **aDOMNodeWithTag );

  static
  NS_METHOD             FindTagInChildren( const char  *aName,
                                           const char  *aNameSpace,
                                           nsIDOMNode  *aDOMNode,
                                           nsIDOMNode **aDOMNodeWithTag );
  static
  NS_METHOD             FindTagInChildren( nsStringArray  *aNameChoices,
                                           const char     *aNameSpace,
                                           nsIDOMNode     *aDOMNode,
                                           nsIDOMNode    **aDOMNodeWithTag );
  static
  NS_METHOD             FindTagInChildren( const char  *aName,
                                           const char  *aNameSpace,
                                           nsIDOMNode  *aDOMNode,
                                           nsIDOMNode **aDOMNodeWithTag,
                                           PRInt32      aStartChild,
                                           PRInt32&     aFoundChild );
  static
  NS_METHOD             FindTagInChildren( nsStringArray  *aNameChoices,
                                           const char     *aNameSpace,
                                           nsIDOMNode     *aDOMNode,
                                           nsIDOMNode    **aDOMNodeWithTag,
                                           PRInt32         aStartChild,
                                           PRInt32&        aFoundChild );

  static
  NS_METHOD             GetName( nsIDOMNode  *aDOMNode,
                                 nsString&    aName );

  static
  NS_METHOD             GetNameSpace( nsIDOMNode  *aDOMNode,
                                      nsString&    aNameSpace );

  static
  NS_METHOD             GetAttributes( nsIDOMNode     *aDOMNode,
                                       nsStringArray  *aAttributeNames,
                                       nsStringArray  *aAttributeNameSpaces,
                                       nsStringArray  *aAttributeValues );

  static
  NS_METHOD             GetText( nsIDOMNode  *aDOMNode,
                                 nsString&    aText );
};

#endif                                           /* nsP3PXMLUtils_h__         */
