/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsBlockDebugFlags_h__
#define nsBlockDebugFlags_h__

#undef NOISY_FIRST_LINE           // enables debug output for first-line specific layout
#undef REALLY_NOISY_FIRST_LINE    // enables extra debug output for first-line specific layout
#undef NOISY_FIRST_LETTER         // enables debug output for first-letter specific layout
#undef NOISY_MAX_ELEMENT_SIZE     // enables debug output for max element size computation
#undef NOISY_MAXIMUM_WIDTH        // enables debug output for max width computation
#undef NOISY_KIDXMOST             // enables debug output for aState.mKidXMost computation
#undef NOISY_FLOATER              // enables debug output for floater reflow (the in/out metrics for the floated block)
#undef NOISY_FLOATER_CLEARING
#undef NOISY_FINAL_SIZE           // enables debug output for desired width/height computation, once all children have been reflowed
#undef NOISY_REMOVE_FRAME
#undef NOISY_COMBINED_AREA        // enables debug output for combined area computation
#undef NOISY_VERTICAL_MARGINS
#undef NOISY_REFLOW_REASON        // gives a little info about why each reflow was requested
#undef REFLOW_STATUS_COVERAGE     // I think this is most useful for printing, to see which frames return "incomplete"
#undef NOISY_SPACEMANAGER         // enables debug output for space manager use, useful for analysing reflow of floaters and positioned elements
#undef NOISY_BLOCK_INVALIDATE     // enables debug output for all calls to invalidate
#undef REALLY_NOISY_REFLOW        // some extra debug info

#endif // nsBlockDebugFlags_h__
