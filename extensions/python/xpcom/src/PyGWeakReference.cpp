/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
 * the specific language governing rights and limitations under the License.
 *
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is ActiveState Tool Corp.
 * Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
 * ActiveState Tool Corp.  All Rights Reserved.
 *
 * Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
 *
 */

// PyGWeakReference - implements weak references for gateways.
//
// This code is part of the XPCOM extensions for Python.
//
// Written November 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#include "PyXPCOM_std.h"

PyXPCOM_GatewayWeakReference::PyXPCOM_GatewayWeakReference( PyG_Base *base )
{
	m_pBase = base;
	NS_INIT_REFCNT();
}

PyXPCOM_GatewayWeakReference::~PyXPCOM_GatewayWeakReference()
{
	// Simply zap my reference to the gateway!
	// No need to zap my gateway's reference to me, as
	// it already holds a reference, so if we are destructing,
	// then it can't possibly hold one.
	m_pBase = NULL;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(PyXPCOM_GatewayWeakReference, nsIWeakReference)

NS_IMETHODIMP
PyXPCOM_GatewayWeakReference::QueryReferent(REFNSIID iid, void * *ret)
{
	{ 
	// Temp scope for lock.  We can't hold the lock during
	// a QI, as this may itself need the lock.
	// Make sure our object isn't dieing right now on another thread.
	CEnterLeaveXPCOMFramework _celf;
	if (m_pBase == NULL)
		return NS_ERROR_NULL_POINTER;
	m_pBase->AddRef(); // Can't die while we have a ref.
	} // end of lock scope - lock unlocked.
	nsresult nr = m_pBase->QueryInterface(iid, ret);
	m_pBase->Release();
	return nr;
}
