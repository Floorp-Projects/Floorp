/* Copyright (c) 2000-2001 ActiveState Tool Corporation.
   See the file LICENSE.txt for licensing information. */

// standard include - sets up all the defines used by
// the mozilla make process - too lazy to work out how to integrate
// with their make, so this will do!

//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

// Main Mozilla cross-platform declarations.
#include "xp_core.h"

#ifdef _DEBUG
#	ifndef DEBUG
#		define DEBUG
#	endif

#	define DEVELOPER_DEBUG
#	define NS_DEBUG
#	define DEBUG_markh
#endif // DEBUG

#include <nsIAllocator.h>
#include <nsIWeakReference.h>
#include <nsXPIDLString.h>
#include <nsCRT.h>
#include <xptcall.h>
#include <xpt_xdr.h>

// This header is considered internal - hence
// we can use it to trigger "exports"
#define BUILD_PYXPCOM

#ifdef HAVE_LONG_LONG
	// Mozilla also defines this - we undefine it to
	// prevent a compiler warning.
#	undef HAVE_LONG_LONG
#endif // HAVE_LONG_LONG

#include "Python.h"
#include "PyXPCOM.h"
