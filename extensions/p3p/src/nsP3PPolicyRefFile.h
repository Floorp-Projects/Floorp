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

#ifndef nsP3PPolicyRefFile_h__
#define nsP3PPolicyRefFile_h__

#include "nsIP3PPolicyRefFile.h"

#include <nsHashtable.h>
#include <nsVoidArray.h>


class nsP3PPolicyRefFile : public nsIP3PPolicyRefFile,
                           public nsIP3PXMLListener {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PPolicyRefFile
  NS_DECL_NSIP3PPOLICYREFFILE

  // nsIP3PXMLListener
  NS_DECL_NSIP3PXMLLISTENER

  // nsP3PXMLProcessor method overrides
  NS_IMETHOD            PostInit( nsString&  aURISpec );

  NS_IMETHOD            ProcessContent( PRInt32   aReadType );

  NS_IMETHOD_( void )   CheckForComplete( );

  // nsP3PPolicyRefFile methods
  nsP3PPolicyRefFile( PRInt32   aReferencePoint );
  virtual ~nsP3PPolicyRefFile( );

protected:
  NS_METHOD_( void )    ProcessPolicyRefFile( PRInt32   aReadType );

  NS_METHOD             ProcessExpiration( );

  NS_METHOD             ProcessPolicyReferencesTagExpiry( nsIP3PTag  *aTag );

  NS_METHOD             ProcessExpiryTagExpiry( nsIP3PTag  *aTag );

  NS_METHOD             ProcessInlinePolicies( PRInt32   aReadType );

  NS_METHOD             ProcessPolicyReferencesTagPolicies( nsIP3PTag  *aTag,
                                                            PRInt32     aReadType );

  NS_METHOD             ProcessPoliciesTagPolicies( nsIP3PTag  *aTag,
                                                    PRInt32     aReadType );

  NS_METHOD             AddInlinePolicy( nsIP3PTag  *aTag,
                                         PRInt32     aReadType );

  NS_METHOD             CheckExpiration( PRBool&  aExpired );

  NS_METHOD_( PRBool )  MatchingAllowed( PRBool     aEmbedCheck,
                                         nsString&  aURIPath );

  NS_METHOD             MatchWithPolicyRefTag( nsIP3PTag  *aTag,
                                               PRBool      aEmbedCheck,
                                               nsString&   aURIMethod,
                                               nsString&   aURIPath,
                                               nsString&   aPolicyURISpec );

  NS_METHOD             MethodMatch( nsISupportsArray  *aTags,
                                     nsString&          aURIMethod,
                                     PRBool&            aMatch );

  NS_METHOD             URIStringMatch( nsISupportsArray  *aTags,
                                        nsString&          aURIString,
                                        PRBool&            aMatch );

  NS_METHOD_( void )    UnEscapeURILegalCharacters( nsString&  aString );

  NS_METHOD_( void )    UnEscapeAsterisks( nsString&  aString );

  NS_METHOD_( void )    UnEscape( nsString&  aString,
                                  nsString&  aEscapeSequence,
                                  nsString&  aCharacter );

  NS_METHOD_( PRUnichar ) CharToNum( PRUnichar   aChar );

  NS_METHOD_( PRBool )  PatternMatch( nsString&  aURIPattern,
                                      nsString&  aURIPath );

  NS_METHOD             CreatePolicyURISpec( nsString&  aInPolicyURISpec,
                                             nsString&  aOutPolicyURISpec );


  PRInt32               mReferencePoint;              // Specifies where the reference to this PolicyRefFile occurred (ie. HTTP header, etc.)

  nsStringArray         mInlinePoliciesToRead;        // The list of Policies required for this PolicyRefFile object

  nsSupportsHashtable   mInlinePolicies;              // The collection of Policy objects created for this PolicyRefFile object

  static
  char                  mURILegalEscapeTable[];
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PPolicyRefFile( nsString&             aPolicyRefFileURISpec,
                                                PRInt32               aReferencePoint,
                                                nsIP3PPolicyRefFile **aPolicyRefFile );

#endif                                           /* nsP3PPolicyRefFile_h__    */
