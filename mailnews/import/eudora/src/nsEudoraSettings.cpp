/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*
  Eudora settings
*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsEudoraSettings.h"
#include "nsEudoraStringBundle.h"

#include "EudoraDebugLog.h"

#ifdef XP_PC
#include "nsEudoraWin32.h"
#endif
#ifdef XP_MAC
#include "nsEudoraMac.h"
#endif


////////////////////////////////////////////////////////////////////////
nsresult nsEudoraSettings::Create(nsIImportSettings** aImport)
{
    NS_PRECONDITION(aImport != nsnull, "null ptr");
    if (! aImport)
        return NS_ERROR_NULL_POINTER;

    *aImport = new nsEudoraSettings();
    if (! *aImport)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aImport);
    return NS_OK;
}

nsEudoraSettings::nsEudoraSettings()
{
	m_pLocation = nsnull;
}

nsEudoraSettings::~nsEudoraSettings()
{
	NS_IF_RELEASE( m_pLocation);
}

NS_IMPL_ISUPPORTS1(nsEudoraSettings, nsIImportSettings)

NS_IMETHODIMP nsEudoraSettings::AutoLocate(PRUnichar **description, nsIFileSpec **location, PRBool *_retval)
{
    NS_PRECONDITION(description != nsnull, "null ptr");
    NS_PRECONDITION(_retval != nsnull, "null ptr");
    NS_PRECONDITION(location != nsnull, "null ptr");
	if (!description || !_retval || !location)
		return( NS_ERROR_NULL_POINTER);
	
	*description = nsnull;	
	*_retval = PR_FALSE;	
	*location = nsnull;

	nsresult	rv;
	if (NS_FAILED( rv = NS_NewFileSpec( location)))
		return( rv);
	*description = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NAME);

#ifdef XP_PC
	*_retval = nsEudoraWin32::FindSettingsFile( *location);
#endif
	
	m_pLocation = *location;
	NS_IF_ADDREF( m_pLocation);

	return( NS_OK);
}

NS_IMETHODIMP nsEudoraSettings::SetLocation(nsIFileSpec *location)
{
	NS_IF_RELEASE( m_pLocation);
	m_pLocation = location;
	NS_IF_ADDREF( m_pLocation);

	return( NS_OK);
}

NS_IMETHODIMP nsEudoraSettings::Import(nsIMsgAccount **localMailAccount, PRBool *_retval)
{
	NS_PRECONDITION( _retval != nsnull, "null ptr");
	nsresult	rv;

	*_retval = PR_FALSE;

	// Get the settings file if it doesn't exist
	if (!m_pLocation) {
#ifdef XP_PC
		if (NS_SUCCEEDED( rv = NS_NewFileSpec( &m_pLocation))) {
			if (!nsEudoraWin32::FindSettingsFile( m_pLocation)) {
				NS_IF_RELEASE( m_pLocation);
				m_pLocation = nsnull;
			}
		}
#endif
#ifdef XP_MAC
		if (NS_SUCCEEDED( rv = NS_NewFileSpec( &m_pLocation))) {
			if (!nsEudoraMac::FindSettingsFile( m_pLocation)) {
				NS_IF_RELEASE( m_pLocation);
				m_pLocation = nsnull;
			}
		}
#endif
	}

	if (!m_pLocation) {
		IMPORT_LOG0( "*** Error, unable to locate settings file for import.\n");
		return( NS_ERROR_FAILURE);
	}

	// do the settings import
#ifdef XP_PC
	*_retval = nsEudoraWin32::ImportSettings( m_pLocation, localMailAccount);
#endif
#ifdef XP_MAC
	*_retval = nsEudoraMac::ImportSettings( m_pLocation, localMailAccount);
#endif

	if (*_retval) {
		IMPORT_LOG0( "Successful import of eudora settings\n");
	}
	else {
		IMPORT_LOG0( "*** Error, Unsuccessful import of eudora settings\n");
	}

	return( NS_OK);
}
