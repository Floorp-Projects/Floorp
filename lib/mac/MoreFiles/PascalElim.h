/*
**	The PascalElim changes to MoreFiles source and header files, along with
**	this file, let you disable Pascal calling conventions in all MoreFiles
**	routines except for system callbacks that require Pascal calling
**	conventions. This will make C programs both smaller and faster.
**
**	Just un-comment "#define __WANTPASCALELIMINATION" in this file to
**	disable Pascal calling conventions
**
**	Changes supplied by Fabrizio Oddone
**
**	File:	PascalElim.h
*/

/* 
 * This code, which was decended from Apple Sample Code, has been modified by 
 * Netscape.
 */

/*	Un-comment "#define __WANTPASCALELIMINATION" (the following line)
**	to disable Pascal calling conventions.
*/
#if 0
#define __WANTPASCALELIMINATION
#endif

#ifdef  __WANTPASCALELIMINATION
#define pascal	
#endif
