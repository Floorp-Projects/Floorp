// ===========================================================================
//	Appearance_DebugHeaders.h	©1996-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================

	// Use PowerPlant-specific Precompiled header
	
#if __POWERPC__
	#include "Appearance_DebugHeadersPPC++"
	
#else
	#include "Appearance_DebugHeaders68K++"
#endif
