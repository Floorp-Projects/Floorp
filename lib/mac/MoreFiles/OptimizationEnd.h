/*
**	Apple Macintosh Developer Technical Support
**
**	DirectoryCopy: #defines that let you make MoreFiles code more efficient.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		OptimizationEnd.h
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
**	this file and Optimization.h, let you optimize the code produced by MoreFiles
**	in several ways.
**
**	Original changes supplied by Fabrizio Oddone
*/


#if	__USEPRAGMAINTERNAL
	#if defined(__MWERKS__)
		#pragma internal reset
	#endif
#endif


#if	__WANTPASCALELIMINATION
	#ifndef __COMPILINGMOREFILES
		#undef pascal
	#endif
#endif
