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
 */

#ifndef nsImportMailboxDescriptor_h___
#define nsImportMailboxDescriptor_h___

#include "nscore.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIFileSpec.h"

////////////////////////////////////////////////////////////////////////


class nsImportMailboxDescriptor : public nsIImportMailboxDescriptor
{
public: 
	NS_DECL_ISUPPORTS

	NS_IMETHOD	GetIdentifier( PRUint32 *pIdentifier) { *pIdentifier = m_id; return( NS_OK);}
	NS_IMETHOD	SetIdentifier( PRUint32 ident) { m_id = ident; return( NS_OK);}
	
	/* attribute unsigned long depth; */
	NS_IMETHOD	GetDepth( PRUint32 *pDepth) { *pDepth = m_depth; return( NS_OK);}
	NS_IMETHOD	SetDepth( PRUint32 theDepth) { m_depth = theDepth; return( NS_OK);}
	
	/* attribute unsigned long size; */
	NS_IMETHOD	GetSize( PRUint32 *pSize) { *pSize = m_size; return( NS_OK);}
	NS_IMETHOD	SetSize( PRUint32 theSize) { m_size = theSize; return( NS_OK);}
	
	/* attribute wstring displayName; */
	NS_IMETHOD	GetDisplayName( PRUnichar **pName) { *pName = m_displayName.ToNewUnicode(); return( NS_OK);}
	NS_IMETHOD	SetDisplayName( const PRUnichar * pName) { m_displayName = pName; return( NS_OK);}
	
	/* attribute boolean import; */
	NS_IMETHOD	GetImport( PRBool *pImport) { *pImport = m_import; return( NS_OK);}
	NS_IMETHOD	SetImport( PRBool doImport) { m_import = doImport; return( NS_OK);}
	
	/* readonly attribute nsIFileSpec fileSpec; */
	NS_IMETHOD GetFileSpec(nsIFileSpec * *aFileSpec) { if (m_pFileSpec) { m_pFileSpec->AddRef(); *aFileSpec = m_pFileSpec; return( NS_OK);} else return( NS_ERROR_FAILURE); }



	nsImportMailboxDescriptor();
	virtual ~nsImportMailboxDescriptor() { if (m_pFileSpec) m_pFileSpec->Release();}

 	static NS_METHOD Create( nsISupports *aOuter, REFNSIID aIID, void **aResult);
			
private:
	PRUint32		m_id;			// used by creator of the structure
	PRUint32		m_depth;		// depth in the heirarchy
	nsString		m_displayName;// name of this mailbox
	nsIFileSpec	*	m_pFileSpec;	// source file (if applicable)
	PRUint32		m_size;
	PRBool			m_import;		// import it or not?	
};


#endif
