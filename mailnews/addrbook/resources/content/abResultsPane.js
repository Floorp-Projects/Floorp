/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Seth Spitzer <sspitzer@netscape.com>
 */

function AbResultsPaneOnClick(event)
{
  dump("XXX onclick\n");
}

function AbResultsPaneKeyPress(event)
{
  dump("XXX key press\n");
}

var gAbResultsOutliner = null;

function GetAbResultsOutliner()
{
  if (gAbResultsOutliner) 
    return gAbResultsOutliner;

	gAbResultsOutliner = document.getElementById('abResultsOutliner');
	return gAbResultsOutliner;
}


function AbResultPaneOnLoad()
{
  var outliner = GetAbResultsOutliner();
  outliner.addEventListener("click",AbResultsPaneOnClick,true);
}

addEventListener("load",AbResultPaneOnLoad,true);
