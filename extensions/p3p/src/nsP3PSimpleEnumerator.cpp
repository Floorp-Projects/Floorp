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

#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"


// ****************************************************************************
// nsP3PSimpleEnumerator Implementation routines
// ****************************************************************************

// P3P Simple Enumerator: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PSimpleEnumerator, nsISimpleEnumerator );

// P3P Simple Enumerator: Creation routine
NS_METHOD
NS_NewP3PSimpleEnumerator( nsISupportsArray     *aSupportsArray,
                           nsISimpleEnumerator **aEnumerator ) {

  nsresult             rv;

  nsISimpleEnumerator *pEnumerator = nsnull;


  NS_ENSURE_ARG_POINTER( aEnumerator );

  *aEnumerator = nsnull;

  pEnumerator = new nsP3PSimpleEnumerator( aSupportsArray );
  if (pEnumerator) {
    rv = pEnumerator->QueryInterface( NS_GET_IID( nsISimpleEnumerator ),
                             (void **)aEnumerator );
  }
  else {
    NS_ASSERTION( 0, "P3P:  SimpleEnumerator unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Simple Enumerator: Constructor
nsP3PSimpleEnumerator::nsP3PSimpleEnumerator( nsISupportsArray  *aArray )
  : mArray( aArray ),
    mIndex( 0 ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Simple Enumerator: Destructor
nsP3PSimpleEnumerator::~nsP3PSimpleEnumerator( ) {
}

// ****************************************************************************
// nsISimpleEnumerator routines
// ****************************************************************************

// P3P Simple Enumerator: HasMoreElements
//
// Function:  Returns an indicator of whether there are more elements in the enumeration.
//
// Parms:     1. Out    An indicator as to whether there are more elements in the enumeration
//
NS_IMETHODIMP
nsP3PSimpleEnumerator::HasMoreElements( PRBool  *aHasMoreElements ) {

  nsresult  rv = NS_OK;

  PRUint32  iCount = 0;


  NS_ENSURE_ARG_POINTER( aHasMoreElements );

  *aHasMoreElements = PR_FALSE;

  if (mArray) {
    rv = mArray->Count(&iCount );

    if (NS_SUCCEEDED( rv ) && (iCount > mIndex)) {
      *aHasMoreElements = PR_TRUE;
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PSimpleEnumerator:  HasMoreElements, mArray->Count failed - %X.\n", rv) );
    }
  }
  else {
    *aHasMoreElements = PR_FALSE;
  }

  return rv;
}

// P3P Simple Enumerator: GetNext
//
// Function:  Returns the next element in the enumeration.
//
// Parms:     1. Out    The next element in the enumeration
//
NS_IMETHODIMP
nsP3PSimpleEnumerator::GetNext( nsISupports **aNextTag ) {

  nsresult  rv = NS_OK;

  PRUint32  iCount = 0;


  NS_ENSURE_ARG_POINTER( aNextTag );

  *aNextTag = nsnull;

  if (mArray) {
    rv = mArray->Count(&iCount );

    if (NS_SUCCEEDED( rv ) && (iCount > mIndex)) {
      rv = mArray->GetElementAt( mIndex,
                                 aNextTag );
      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PSimpleEnumerator:  HasMoreElements, mArray->GetElementAt position %i failed - %X.\n", mIndex, rv) );
      }

      mIndex++;
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PSimpleEnumerator:  HasMoreElements, mArray->Count failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PSimpleEnumerator:  HasMoreElements, mArray is null.\n") );

    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}
