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

#include "nsP3PObserverUtils.h"
#include "nsP3PLogging.h"

#include <nsString.h>
#include <nsXPIDLString.h>


// P3P Observer Utils: ExamineLINKTag
//
// Function:  Examines the content of an HTML <LINK> tag.
//
// Parms:     1. In     The URI object associated with the HTML <LINK> tag
//            2. In     The number of attributes associated with the <LINK> tag
//            3. In     The attribute names
//            4. In     The attribute values
//            5. In     The P3P Service
//
NS_METHOD
nsP3PObserverUtils::ExamineLINKTag( nsIURI               *aReferencingURI,
                                    PRUint32              aNumOfAttributes,
                                    const PRUnichar      *aNameArray[],
                                    const PRUnichar      *aValueArray[],
                                    nsIDocShellTreeItem  *aDocShellTreeItem,
                                    nsIP3PCService       *aP3PService ) {

  nsAutoString  sPolicyRefFileURISpec;


#ifdef DEBUG_P3P
  { nsXPIDLCString  xcsURISpec;

    aReferencingURI->GetSpec( getter_Copies( xcsURISpec ) );
    printf( "      Referencing URI: %s\n", (const char *)xcsURISpec );
  }
#endif

  for (PRUint32 i = 0;
         i < aNumOfAttributes;
           i++) {
    if (nsCRT::strcasecmp( aNameArray[i], P3P_LINK_REL ) == 0) {

      if (nsCRT::strcmp( aValueArray[i], P3P_LINK_P3P_VALUE ) == 0) {

        for (PRUint32 j = 0;
               j < aNumOfAttributes;
                 j++) {
          if (nsCRT::strcasecmp( aNameArray[j], P3P_LINK_HREF ) == 0) {
            sPolicyRefFileURISpec = aValueArray[j];
            sPolicyRefFileURISpec.Trim( "\"" );
#ifdef DEBUG_P3P
            { nsCString  csRel,
                         csHref;

              csRel.AssignWithConversion( aValueArray[i] );
              csHref.AssignWithConversion( aValueArray[j] );
              printf( "      P3P LINK tag found: %s  %s\n", (const char *)csRel,
                                                            (const char *)csHref );
            }
#endif
          } 
        }
      }

      if (sPolicyRefFileURISpec.Length( ) > 0) {
        aP3PService->SetPolicyRefFileURISpec( aDocShellTreeItem,
                                              aReferencingURI,
                                              P3P_REFERENCE_LINK_TAG,
                                              sPolicyRefFileURISpec );
      }
    }
  }

  return NS_OK;
}

// P3P Observer Utils: ExamineLINKTag
//
// Function:  Examines the content of an HTML <LINK> tag.
//
// Parms:     1. In     The URI object associated with the HTML <LINK> tag
//            2. In     The attribute names
//            3. In     The attribute values
//            4. In     The P3P Service
//
NS_METHOD
nsP3PObserverUtils::ExamineLINKTag( nsIURI               *aReferencingURI,
                                    const nsStringArray  *aNames,
                                    const nsStringArray  *aValues,
                                    nsIDocShellTreeItem  *aDocShellTreeItem,
                                    nsIP3PCService       *aP3PService ) {

  nsAutoString   sName,
                 sRelValue,
                 sHrefValue,
                 sPolicyRefFileURISpec;

  nsCAutoString  csHref;


#ifdef DEBUG_P3P
  { nsXPIDLCString  xcsURISpec;

    aReferencingURI->GetSpec( getter_Copies( xcsURISpec ) );
    printf( "      Referencing URI: %s\n", (const char *)xcsURISpec );
  }
#endif

  // Loop through the attribute names looking for the "rel" attribute with the P3P release value
  for (PRInt32 i = 0;
         i < aNames->Count( );
           i++) {
    aNames->StringAt( i, sName );

    if (sName.EqualsIgnoreCase( P3P_LINK_REL )) {
      // "rel" attribute found
      aValues->StringAt( i, sRelValue );

      if (sRelValue.EqualsWithConversion( P3P_LINK_P3P_VALUE )) {
        // P3P release value found
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PObserverUtils:  ExamineLinkTag, P3P rel attribute found - %s.\n", P3P_LINK_P3P_VALUE) );

        // Loop through the attributes names looking for the "href" attribute with the PolicyRefFile value
        for (PRInt32 j = 0;
               j < aNames->Count( );
                 j++) {
          aNames->StringAt( j, sName );

          if (sName.EqualsIgnoreCase( P3P_LINK_HREF )) {
            // "href" attribute found, set PolicyRefFile URI spec
            aValues->StringAt( j, sHrefValue );
            csHref.AssignWithConversion( sHrefValue );
            sPolicyRefFileURISpec = sHrefValue;
            sPolicyRefFileURISpec.Trim( "\"" );

#ifdef DEBUG_P3P
            { nsCString  csRel;

              csRel.AssignWithConversion( sRelValue );
              printf( "      P3P LINK tag found: %s  %s\n", (const char *)csRel, (const char *)csHref );
            }
#endif

            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PObserverUtils:  ExamineLinkTag, P3P href attribute found - %s.\n", (const char *)csHref) );
          }
        }
      }

      if (sPolicyRefFileURISpec.Length( ) > 0) {
        aP3PService->SetPolicyRefFileURISpec( aDocShellTreeItem,
                                              aReferencingURI,
                                              P3P_REFERENCE_LINK_TAG,
                                              sPolicyRefFileURISpec );
      }
    }
  }

  return NS_OK;
}
