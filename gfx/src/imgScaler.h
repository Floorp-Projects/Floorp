/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Tomas Mšller
 * Portions created by Tomas Mšller are 
 * Copyright (C) 2001 Tomas Mšller.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tomas Mšller
 *   Tim Rowley <tor@cs.brown.edu>
 */

#include "nsComObsolete.h"

NS_GFX_(void)
RectStretch(unsigned aSrcWidth, unsigned aSrcHeight,
	    unsigned aDstWidth, unsigned aDstHeight,
	    unsigned aStartColumn, unsigned aStartRow,
	    unsigned aEndColumn, unsigned aEndRowe,
	    unsigned char *aSrcImage, unsigned aSrcStride,
	    unsigned char *aDstImage, unsigned aDstStride,
	    unsigned aDepth);
