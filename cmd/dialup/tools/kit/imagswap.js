<!--  -*- Mode: HTML; tab-width: 2; indent-tabs-mode: nil -*-

   The contents of this file are subject to the Netscape Public License
   Version 1.0 (the "NPL"); you may not use this file except in
   compliance with the NPL.  You may obtain a copy of the NPL at
   http://www.mozilla.org/NPL/

   Software distributed under the NPL is distributed on an "AS IS" basis,
   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
   for the specific language governing rights and limitations under the
   NPL.

   The Initial Developer of this code under the NPL is Netscape
   Communications Corporation.  Portions created by Netscape are
   Copyright (C) 1998 Netscape Communications Corporation.  All Rights
   Reserved.
 -->
//imageswap.js
//functions to swap the src and lowsrc of an image

function swapSrc(imageRef)
{
	if ((imageRef) && (imageRef != null))
	{
		if (imageRef.lowsrc && imageRef.src);
		{
		
			var lsrc = imageRef.lowsrc;
			var src = imageRef.src;
			
			imageRef.lowsrc = src;
			imageRef.src = lsrc;	
		}
	}
}

function replaceSrc(imageRef, newimg)
{
	if ((imageRef) && (imageRef != null))
	{
		if ((newimg) && (newimg != null) && (newimg != ""));
		{
				imageRef.src = newimg;	
		}
	}
}
