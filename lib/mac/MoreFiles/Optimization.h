/*
**	Apple Macintosh Developer Technical Support
**
**	DirectoryCopy: #defines that let you make MoreFiles code more efficient.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		Optimization.h
**
**	Copyright © 1992-1998 Apple Computer, Inc.
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.  However, what you are
**	not permitted to do is to redistribute the source as "DSC Sample Code"
**	after having made changes. If you're going to re-distribute the source,
**	we require that you make it clear in the source that the code was
**	descended from Apple Sample Code, but that you've made changes.
**
**	The Optimization changes to MoreFiles source and header files, along with
**	this file and OptimizationEnd.h, let you optimize the code produced
**	by MoreFiles in several ways.
**
**	1 -- MoreFiles contains extra code so that many routines can run under
**	Mac OS systems back to System 6. If your program requires a specific
**	version of Mac OS and your program checks for that version before
**	calling MoreFiles routines, then you can remove a lot of compatibility
**	code by defining one of the following to 1:
**
**		__MACOSSEVENFIVEONEORLATER	// assume Mac OS 7.5.1 or later
**		__MACOSSEVENFIVEORLATER		// assume Mac OS 7.5 or later
**		__MACOSSEVENORLATER			// assume Mac OS 7.0 or later
**
**	By default, all compatibility code is ON.
**
**	2 -- You may disable Pascal calling conventions in all MoreFiles routines
**	except for system callbacks that require Pascal calling conventions.
**	This will make C programs both smaller and faster.
**	Just define __WANTPASCALELIMINATION to be 1 to turn this optimization on
**	when building MoreFiles for use from C programs (you'll need to keep
**	Pascal calling conventions when linking MoreFiles routines with Pascal
**	programs).
**
**	3 -- If Metrowerks compiler is used, "#pragma internal on" may help produce
**	better code. However, this option can also cause problems if you're
**	trying to build MoreFiles as a shared library, so it is by default not used.
**	Just define __USEPRAGMAINTERNAL to be 1 to turn this optimization on.
**
**	Original changes supplied by Fabrizio Oddone
**
**	File:	Optimization.h
*/


/* 
 * This code, which was decended from Apple Sample Code, has been modified by 
 * Netscape.
 */

/* 
 * For Netscape purposes we want __MACOSSEVENFIVEONEORLATER to be turned on by default
 * We also want to turn on __WANTPASCALELIMINATION but that will require touching files
 * that I don't want to change right now
 */
#define	__MACOSSEVENFIVEONEORLATER	1

#ifndef __MACOSSEVENFIVEONEORLATER
	#define __MACOSSEVENFIVEONEORLATER 0
#endif

#ifndef __MACOSSEVENFIVEORLATER
	#define __MACOSSEVENFIVEORLATER __MACOSSEVENFIVEONEORLATER
#endif

#ifndef __MACOSSEVENORLATER
	#if TARGET_RT_MAC_CFM
		#define __MACOSSEVENORLATER 1
	#else
		#define __MACOSSEVENORLATER __MACOSSEVENFIVEORLATER
	#endif
#endif


#ifndef	__WANTPASCALELIMINATION
	#define	__WANTPASCALELIMINATION	0
#endif

#if	__WANTPASCALELIMINATION
	#define pascal	
#endif


#ifndef __USEPRAGMAINTERNAL
	#define	__USEPRAGMAINTERNAL	0
#endif

#if	__USEPRAGMAINTERNAL
	#if defined(__MWERKS__)
		#pragma internal on
	#endif
#endif

