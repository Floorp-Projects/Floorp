/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Patrick Beard
 */

#include "nsIScriptableRegion.h"
#include "nsComObsolete.h"

class nsIRegion;

/**
 * An adapter class for the unscriptable nsIRegion interface.
 */
class NS_GFX nsScriptableRegion : public nsIScriptableRegion {
public:
	nsScriptableRegion(nsIRegion* region);
	virtual ~nsScriptableRegion();
	
	NS_DECL_ISUPPORTS

	NS_DECL_NSISCRIPTABLEREGION

private:
	nsIRegion* mRegion;
};
