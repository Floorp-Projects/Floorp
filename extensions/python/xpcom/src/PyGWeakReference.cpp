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

#ifdef NS_BUILD_REFCNT_LOGGING
	// bloat view uses 40 chars - stick "(WR)" at the end of this position.
	strncpy(refcntLogRepr, m_pBase->refcntLogRepr, sizeof(refcntLogRepr));
	refcntLogRepr[sizeof(refcntLogRepr)-1] = '\0';
	char *dest = refcntLogRepr + ((strlen(refcntLogRepr) > 36) ? 36 : strlen(refcntLogRepr));
	strcpy(dest, "(WR)");
#endif // NS_BUILD_REFCNT_LOGGING
}

PyXPCOM_GatewayWeakReference::~PyXPCOM_GatewayWeakReference()
{
	// Simply zap my reference to the gateway!
	// No need to zap my gateway's reference to me, as
	// it already holds a reference, so if we are destructing,
	// then it can't possibly hold one.
	m_pBase = NULL;
}

nsrefcnt
PyXPCOM_GatewayWeakReference::AddRef(void)
{
	nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
#ifdef NS_BUILD_REFCNT_LOGGING
	NS_LOG_ADDREF(this, cnt, refcntLogRepr, sizeof(*this));
#endif
	return cnt;
}

nsrefcnt
PyXPCOM_GatewayWeakReference::Release(void)
{
	nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
#ifdef NS_BUILD_REFCNT_LOGGING
	NS_LOG_RELEASE(this, cnt, refcntLogRepr);
#endif
	if ( cnt == 0 )
		delete this;
	return cnt;
}

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(PyXPCOM_GatewayWeakReference, nsIWeakReference)

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
