// ===========================================================================
//	CAppearanceApp.h			©1994-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================

#pragma once

#include <PP_Prefix.h>
#include <LApplication.h>


class	CAppearanceApp : public PP_PowerPlant::LApplication {

public:
							CAppearanceApp();	// constructor registers PPobs
		virtual 			~CAppearanceApp();	// stub destructor
	
	
		// this overriding method handles application commands
		virtual Boolean		ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam);	
	
	
		// this overriding method returns the status of menu items
		virtual void		FindCommandStatus(PP_PowerPlant::CommandT inCommand,
									Boolean &outEnabled, Boolean &outUsesMark,
									PP_PowerPlant::Char16 &outMark, Str255 outName);
protected:

		virtual void		StartUp();			// override startup functions

};