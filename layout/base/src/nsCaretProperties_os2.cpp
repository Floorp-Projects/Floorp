/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1998 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "nscore.h"
#include "nsCaretProperties.h"

#define INCL_WINSYS
#include <os2.h>

nsCaretProperties::nsCaretProperties()
{
   // Sigh - this is in 'twips', should be in 'app units', but there's no
   // DC anyway so we can't work it out!

   ULONG ulPels = WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER);

   // With luck, either:
   //   * these metrics will go into nsILookAndFeel, or
   //   * we'll get an nsIDeviceContext here
   //
   // For now, lets assume p2t = 20.
   mCaretWidth = 20 * ulPels;

   mBlinkRate = WinQuerySysValue( HWND_DESKTOP, SV_CURSORRATE);
}


nsCaretProperties* NewCaretProperties()
{
	return new nsCaretProperties();
}
