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

#include "nsP3PDefines.h"

#include "nsP3PXMLUtils.h"

#include "nsIComponentManager.h"

#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"

#include "nsITextContent.h"

#include "nsXPIDLString.h"


// ****************************************************************************
// nsP3PXMLUtils routines
// ****************************************************************************

// P3P XML Utils: FindTag
//
// Function:  Finds a specified tag.
//
// Parms:     1. In     The name of the tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//
NS_METHOD
nsP3PXMLUtils::FindTag( const char  *aName,
                        const char  *aNameSpace,
                        nsIDOMNode  *aDOMNode,
                        nsIDOMNode **aDOMNodeWithTag ) {

  nsresult       rv;

  nsAutoString   sName;

  nsStringArray  arrayNameChoices;


  sName.AssignWithConversion( aName );

  if (arrayNameChoices.AppendString( sName )) {
    rv = FindTag(&arrayNameChoices,
                  aNameSpace,
                  aDOMNode,
                  aDOMNodeWithTag );
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// P3P XML Utils: FindTag
//
// Function:  Finds one of a number of specified tags.
//
// Parms:     1. In     The list of names of a tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//
NS_METHOD
nsP3PXMLUtils::FindTag( nsStringArray  *aNameChoices,
                        const char     *aNameSpace,
                        nsIDOMNode     *aDOMNode,
                        nsIDOMNode    **aDOMNodeWithTag ) {

  nsresult      rv;

  nsAutoString  sTagNameSpace;

  nsAutoString  sTagName,
                sName;

  PRBool        bFound          = PR_FALSE;


  NS_ENSURE_ARG_POINTER( aDOMNodeWithTag );

  *aDOMNodeWithTag = nsnull;

  rv = GetName( aDOMNode,
                sTagName );

  for (PRInt32 iCount = 0;
         NS_SUCCEEDED( rv ) && !bFound && (iCount < aNameChoices->Count( ));
           iCount++) {
    aNameChoices->StringAt( iCount,
                            sName );

    if (sTagName == sName) {
      rv = GetNameSpace( aDOMNode,
                         sTagNameSpace );

      if (NS_SUCCEEDED( rv ) && sTagNameSpace.EqualsWithConversion( aNameSpace )) {
#ifdef DEBUG_P3P
        printf( "P3P:    Tag found: %s\n",
                 NS_LossyConvertUCS2toASCII(sName).get() );
#endif

        bFound = PR_TRUE;

        *aDOMNodeWithTag = aDOMNode;
        NS_ADDREF( *aDOMNodeWithTag );
      }
    }
  }

  return rv;
}

// P3P XML Utils: FindTagInChildren
//
// Function:  Finds a specified tag in the children of a DOMNode object.
//
// Parms:     1. In     The name of the tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//
NS_METHOD
nsP3PXMLUtils::FindTagInChildren( const char  *aName,
                                  const char  *aNameSpace,
                                  nsIDOMNode  *aDOMNode,
                                  nsIDOMNode **aDOMNodeWithTag ) {

  nsresult       rv;

  nsAutoString   sName;

  nsStringArray  arrayNameChoices;


  sName.AssignWithConversion( aName );

  if (arrayNameChoices.AppendString( sName )) {
    rv = FindTagInChildren(&arrayNameChoices,
                            aNameSpace,
                            aDOMNode,
                            aDOMNodeWithTag );
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// P3P XML Utils: FindTagInChildren
//
// Function:  Finds a specified tag in the children of a DOMNode object.
//
// Parms:     1. In     The list of names of a tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//
NS_METHOD
nsP3PXMLUtils::FindTagInChildren( nsStringArray  *aNameChoices,
                                  const char     *aNameSpace,
                                  nsIDOMNode     *aDOMNode,
                                  nsIDOMNode    **aDOMNodeWithTag ) {

  nsresult  rv;

  PRInt32   iStartChild = 0,
            iFoundChild = 0;


  rv = FindTagInChildren( aNameChoices,
                          aNameSpace,
                          aDOMNode,
                          aDOMNodeWithTag,
                          iStartChild,
                          iFoundChild );

  return rv;
}

// P3P XML Utils: FindTagInChildren
//
// Function:  Finds a specified tag in the children of a DOMNode object.
//
// Parms:     1. In     The name of the tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//            5. In     The child to begin the search with
//            6. Out    The child in which the tag was found
//
NS_METHOD
nsP3PXMLUtils::FindTagInChildren( const char  *aName,
                                  const char  *aNameSpace,
                                  nsIDOMNode  *aDOMNode,
                                  nsIDOMNode **aDOMNodeWithTag,
                                  PRInt32      aStartChild,
                                  PRInt32&     aFoundChild ) {

  nsresult       rv;

  nsAutoString   sName;

  nsStringArray  arrayNameChoices;


  sName.AssignWithConversion( aName );

  if (arrayNameChoices.AppendString( sName )) {
    rv = FindTagInChildren(&arrayNameChoices,
                            aNameSpace,
                            aDOMNode,
                            aDOMNodeWithTag,
                            aStartChild,
                            aFoundChild );
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// P3P XML Utils: FindTagInChildren
//
// Function:  Finds a specified tag in the children of a DOMNode object.
//
// Parms:     1. In     The list of names of a tag to find
//            2. In     The namespace of the tag to find
//            3. In     The DOMNode object in which to begin the search
//            4. Out    The DOMNode object that represents the tag
//            5. In     The child to begin the search with
//            6. Out    The child in which the tag was found
//
NS_METHOD
nsP3PXMLUtils::FindTagInChildren( nsStringArray  *aNameChoices,
                                  const char     *aNameSpace,
                                  nsIDOMNode     *aDOMNode,
                                  nsIDOMNode    **aDOMNodeWithTag,
                                  PRInt32         aStartChild,
                                  PRInt32&        aFoundChild ) {

  nsresult                  rv;

  PRBool                    bHasChildren;

  PRUint32                  uiChildren;

  nsCOMPtr<nsIDOMNodeList>  pDOMNodeList;
  nsCOMPtr<nsIDOMNode>      pDOMNode;

  PRBool                    bFound           = PR_FALSE;


  NS_ENSURE_ARG_POINTER( aDOMNodeWithTag );

  *aDOMNodeWithTag = nsnull;

  rv = aDOMNode->HasChildNodes(&bHasChildren );

  if (NS_SUCCEEDED( rv ) && bHasChildren) {
    rv = aDOMNode->GetChildNodes( getter_AddRefs( pDOMNodeList ) );

    if (NS_SUCCEEDED( rv ) && pDOMNodeList) {
      rv = pDOMNodeList->GetLength(&uiChildren );

      for (PRUint32 uiCount = (PRUint32)aStartChild;
             NS_SUCCEEDED( rv ) && !bFound && (uiCount < uiChildren);
               uiCount++) {
        rv = pDOMNodeList->Item( uiCount,
                                 getter_AddRefs( pDOMNode ) );

        if (NS_SUCCEEDED( rv ) && pDOMNode) {
          rv = FindTag( aNameChoices,
                        aNameSpace,
                        pDOMNode,
                        aDOMNodeWithTag );

          if (NS_SUCCEEDED( rv ) && *aDOMNodeWithTag) {
            bFound = PR_TRUE;
            aFoundChild = (PRInt32)uiCount;
          }
        }
      }
    }
  }

  return rv;
}

// P3P XML Utils: GetName
//
// Function:  Returns the tag name of the DOMNode object.
//
// Parms:     1. In     The DOMNode object
//            2. Out    The tag name
//
NS_METHOD
nsP3PXMLUtils::GetName( nsIDOMNode  *aDOMNode,
                        nsString&    aName ) {

  nsresult  rv;


#ifdef DEBUG_P3P
  { nsAutoString   sBuf1, sBuf2;
    nsCAutoString  csBuf;

    aDOMNode->GetNodeName( sBuf1 );
    csBuf.AssignWithConversion( sBuf1 );
//    printf( "***  GetNodeName:  %s\n", (const char *)xcsBuf );
    aDOMNode->GetLocalName( sBuf2 );
    csBuf.AssignWithConversion( sBuf2 );
//    printf( "***  GetLocalName: %s\n", (const char *)xcsBuf );
  }
#endif

  rv = aDOMNode->GetLocalName( aName );

  return rv;
}

// P3P XML Utils: GetNameSpace
//
// Function:  Returns the tag namespace of the DOMNode object.
//
// Parms:     1. In     The DOMNode object
//            2. Out    The tag namespace
//
NS_METHOD
nsP3PXMLUtils::GetNameSpace( nsIDOMNode  *aDOMNode,
                             nsString&    aNameSpace ) {

  nsresult  rv;


  rv = aDOMNode->GetNamespaceURI( aNameSpace );

  return rv;
}

// P3P XML Utils: GetAttributes
//
// Function:  Returns all of the tag attributes of the DOMNode object.
//
// Parms:     1. In     The DOMNode object
//            2. Out    The tag attribute names
//            3. Out    The tag attribute namespaces
//            4. Out    The tag attribute values
//
NS_METHOD
nsP3PXMLUtils::GetAttributes( nsIDOMNode     *aDOMNode,
                              nsStringArray  *aAttributeNames,
                              nsStringArray  *aAttributeNameSpaces,
                              nsStringArray  *aAttributeValues ) {
                              
  nsresult                      rv;

  nsCOMPtr<nsIDOMNamedNodeMap>  pDOMAttributeNodes;
  nsCOMPtr<nsIDOMNode>          pDOMNode;

  nsCOMPtr<nsIDOMAttr>          pDOMAttr;

  PRUint32                      uiAttributes;

  nsAutoString                  sAttributeName,
                                sAttributeNameSpace,
                                sAttributeValue;


  rv = aDOMNode->GetAttributes( getter_AddRefs( pDOMAttributeNodes ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pDOMAttributeNodes->GetLength(&uiAttributes );

    if (NS_SUCCEEDED( rv )) {

      for (PRUint32 uiCount = 0;
               NS_SUCCEEDED( rv ) && (uiCount < uiAttributes);
                 uiCount++) {
        rv = pDOMAttributeNodes->Item( uiCount,
                                       getter_AddRefs( pDOMNode ) );

        if (NS_SUCCEEDED( rv ) && pDOMNode) {
          pDOMAttr = do_QueryInterface( pDOMNode,
                                       &rv );

          if (NS_SUCCEEDED( rv ) && pDOMAttr) {
            rv = GetName( pDOMAttr,
                          sAttributeName );

            if (NS_SUCCEEDED( rv )) {
              rv = GetNameSpace( pDOMAttr,
                                 sAttributeNameSpace );

              if (NS_SUCCEEDED( rv )) {
                rv = pDOMAttr->GetValue( sAttributeValue );

                if (NS_SUCCEEDED( rv )) {

                  if (sAttributeName.Find( ":" ) >= 0) {
                    nsAutoString  sTemp;

                    sAttributeName.Right( sTemp, sAttributeName.Length( ) - (sAttributeName.Find( ":" ) + 1) );
                    sAttributeName = sTemp;
                  }

                  if (!aAttributeNames->AppendString( sAttributeName )) {
                    rv = NS_ERROR_FAILURE;
                  }
                  if (!aAttributeNameSpaces->AppendString( sAttributeNameSpace )) {
                    rv = NS_ERROR_FAILURE;
                  }
                  if (!aAttributeValues->AppendString( sAttributeValue )) {
                    rv = NS_ERROR_FAILURE;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return rv;
}

// P3P XML Utils: GetText
//
// Function:  Returns the text associated with the tag of the DOMNode object.
//
// Parms:     1. In     The DOMNode object
//            2. Out    The tag text
//
NS_METHOD
nsP3PXMLUtils::GetText( nsIDOMNode  *aDOMNode,
                        nsString&    aText ) {

  nsresult                  rv;

  PRBool                    bHasChildren;

  PRUint32                  uiChildren;

  nsCOMPtr<nsIDOMNodeList>  pDOMNodeList;
  nsCOMPtr<nsIDOMNode>      pDOMNode;

  nsCOMPtr<nsIDOMText>      pDOMTextElement;

  nsAutoString              sTagText,
                            sText;


  rv = aDOMNode->HasChildNodes(&bHasChildren );

  if (NS_SUCCEEDED( rv ) && bHasChildren) {
    rv = aDOMNode->GetChildNodes( getter_AddRefs( pDOMNodeList ) );

    if (NS_SUCCEEDED( rv ) && pDOMNodeList) {
      rv = pDOMNodeList->GetLength(&uiChildren );

      for (PRUint32 uiCount = 0;
             NS_SUCCEEDED( rv ) && (uiCount < uiChildren);
               uiCount++) {
        rv = pDOMNodeList->Item( uiCount,
                                 getter_AddRefs( pDOMNode ) );

        if (NS_SUCCEEDED( rv ) && pDOMNode) {
          pDOMTextElement = do_QueryInterface( pDOMNode,
                                              &rv );

          if (NS_SUCCEEDED( rv ) && pDOMTextElement) {
            rv = pDOMTextElement->GetData( sText );

            if (NS_SUCCEEDED( rv )) {
              sText.Trim( " \n" );
              sTagText.Append( sText );
            }
          }
          else {
            rv = NS_OK; // Reset return code if this is not a TextElement
          }
        }
      }
    }
  }

  aText = sTagText;

  return rv;
}
