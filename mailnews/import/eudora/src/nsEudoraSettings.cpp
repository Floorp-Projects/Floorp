/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
  Eudora settings
*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsEudoraSettings.h"
#include "nsEudoraStringBundle.h"

#include "EudoraDebugLog.h"

#if defined(XP_WIN) || defined(XP_OS2)
#include "nsEudoraWin32.h"
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
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
}

nsEudoraSettings::~nsEudoraSettings()
{
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

	nsresult	rv;

	if (NS_FAILED( rv = NS_NewFileSpec( getter_AddRefs(m_pLocation))))
		return rv;
	*description = nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NAME);

#if defined(XP_WIN) || defined(XP_OS2)
	*_retval = nsEudoraWin32::FindSettingsFile( m_pLocation );
#endif
	
  NS_IF_ADDREF(*location = m_pLocation);
	return NS_OK;
}

NS_IMETHODIMP nsEudoraSettings::SetLocation(nsIFileSpec *location)
{
	m_pLocation = location;
	return( NS_OK);
}

NS_IMETHODIMP nsEudoraSettings::Import(nsIMsgAccount **localMailAccount, PRBool *_retval)
{
	NS_PRECONDITION( _retval != nsnull, "null ptr");
	nsresult	rv;

	*_retval = PR_FALSE;

	// Get the settings file if it doesn't exist
	if (!m_pLocation) {
#if defined(XP_WIN) || defined(XP_OS2)
		if (NS_SUCCEEDED( rv = NS_NewFileSpec( getter_AddRefs(m_pLocation)))) {
			if (!nsEudoraWin32::FindSettingsFile( m_pLocation)) {
				m_pLocation = nsnull;
			}
		}
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
		if (NS_SUCCEEDED( rv = NS_NewFileSpec( getter_AddRefs(m_pLocation)))) {
			if (!nsEudoraMac::FindSettingsFile( m_pLocation)) {
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
#if defined(XP_WIN) || defined(XP_OS2)
	*_retval = nsEudoraWin32::ImportSettings( m_pLocation, localMailAccount);
#endif
#if defined(XP_MAC) || defined(XP_MACOSX)
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
