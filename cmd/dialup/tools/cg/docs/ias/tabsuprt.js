/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
//tabs.js
// a set of functions for making a set of tabs in one frame that control the content of another frame


//tab images have to have names like: 
//tabnamea.gif (active)
//tabnamei.gif (inactive)



//swaps the lowsrc and src images of an image reference
function swapSrcLowsrc(imageRef)
{
	//FIXME FIXME: THIS FUCNTION HAS A TEMPORARY FIX FOR A BUG IN DOGBERT.
	//FIXME WHEN IT'S RESOLVED BUG # 69298

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


//given a link object, tells you which link it is in the document.
function findLinkIndex(linkRef)
{

	var curIdx = -1;

	if ((linkRef) && (linkRef != null))
	{
		for (curIdx = 0; curIdx < document.links.length; curIdx++)
		{
		
			if (document.links[curIdx] == linkRef)
				return curIdx;
		}
	}

	return -1;

}



//THIS IS A FUCKED UP HACK.
// there was no good way to tell which image was last on, without making some sort of persistent data
//storage, so this is the hack.
// We already assume that there is a 1-to-1 correspondence between the links  array and the
//image array in the document.
//SO, we go to the links array, check the target's location, and see
// if that is equivalent to the link location.  IF SO, we assume it IS ON.
// else it is off.
function imageIsOn(imageRef)
{
	var out = false;
	
	var dot = imageRef.src.lastIndexOf(".");
	
	var indicator = imageRef.src.substring(dot-1, dot);
	
	//alert("dot: " + dot + " indicator: " + indicator + " src: " + imageRef.src);
	
	if (indicator == "a")
		out = true;


	return out;
}


//checks the name property of the passed in image object, if it is "ON", swaps the images.
// if it is anything else, does nothing
function swapImageIfOn(imageIndex, imageRef)
{
	if ((imageRef) && (imageRef != null))
	{
		if ((imageRef.name))
		{
			if (imageIsOn(imageRef))
			{
				swapSrcLowsrc(imageRef);
				//alert("Image " + imageIndex + " was on.");
				
				return true;
			}
		}
	}
	return false;
}



function tabClicked(linkRef)
{

	var theIndex = findLinkIndex(linkRef);
	var onImage = document.images[theIndex];
	
	//first see if the tab that was clicked is the currently active tab, if so, abort
	//FIXME FIXME: THIS FUCNTION HAS A TEMPORARY FIX FOR A BUG IN DOGBERT.
	//FIXME WHEN IT'S RESOLVED BUG # 69298
	if (imageIsOn(onImage))
		return false;


	//second do a checkdata, if such a function exists in the tabbody
	if (parent.tabbody && parent.tabbody.document && parent.tabbody.checkData)
	{
		var checkResult = parent.tabbody.checkData();
		if (checkResult != true)
			return checkResult;
	}


	//alert("tabsuprt: checkresult was true");	
	
	
	var wason = false;
	var target = null;
	
	if ((theIndex >= 0) && (onImage) && (onImage != null))
	{
		//found our image,
		
		//turn all images off
		for (var curImageIndex = 0; curImageIndex < document.images.length; curImageIndex++)
		{
			wason = swapImageIfOn(curImageIndex, document.images[curImageIndex]);	
			
			//this is the funny thing we do to save the data if a savedata event handler is there
			if (wason == true)
			{
				target = eval("parent." + document.links[curImageIndex].target);
				if ((target) && (target != null) && (target.saveData))
				{
					//alert("tabsuprt: savedata");
					target.saveData();
					//alert("tabsuprt: savedata finished");
				}
			
			
			}
		}

		//turn this image visually on
		swapSrcLowsrc(onImage);	
	}

	return true;
}
