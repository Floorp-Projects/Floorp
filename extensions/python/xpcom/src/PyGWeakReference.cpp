/* Copyright (c) 2000-2001 ActiveState Tool Corporation.
   See the file LICENSE.txt for licensing information. */

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

NS_IMPL_THREADSAFE_ADDREF(PyXPCOM_GatewayWeakReference);
NS_IMPL_THREADSAFE_RELEASE(PyXPCOM_GatewayWeakReference);
NS_IMPL_THREADSAFE_QUERY_INTERFACE(PyXPCOM_GatewayWeakReference, NS_GET_IID(nsIWeakReference));

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
