/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"

#include "inDOMView.h"
#include "inDeepTreeWalker.h"
#include "inFlasher.h"
#include "inCSSValueSearch.h"
#include "inFileSearch.h"
#include "inDOMUtils.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMView)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDeepTreeWalker)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFlasher)
NS_GENERIC_FACTORY_CONSTRUCTOR(inCSSValueSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFileSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inDOMUtils)

// {FB5C1775-1BBD-4b9c-ABB0-AE7ACD29E87E}
#define IN_DOMVIEW_CID \
{ 0xfb5c1775, 0x1bbd, 0x4b9c, { 0xab, 0xb0, 0xae, 0x7a, 0xcd, 0x29, 0xe8, 0x7e } }

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID \
{ 0xbfcb82c2, 0x5611, 0x4318, { 0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 } }

// {4D977F60-FBE7-4583-8CB7-F5ED882293EF}
#define IN_CSSVALUESEARCH_CID \
{ 0x4d977f60, 0xfbe7, 0x4583, { 0x8c, 0xb7, 0xf5, 0xed, 0x88, 0x22, 0x93, 0xef } }

// {9286E71A-621A-4b91-851E-9984C1A2E81A}
#define IN_FLASHER_CID \
{ 0x9286e71a, 0x621a, 0x4b91, { 0x85, 0x1e, 0x99, 0x84, 0xc1, 0xa2, 0xe8, 0x1a } }

// {D5636476-9F94-47f2-9CE9-69CDD9D7BBCD}
#define IN_FILESEARCH_CID \
{ 0xd5636476, 0x9f94, 0x47f2, { 0x9c, 0xe9, 0x69, 0xcd, 0xd9, 0xd7, 0xbb, 0xcd } }

// {40B22006-5DD5-42f2-BFE7-7DBF0757AB8B}
#define IN_DOMUTILS_CID \
{ 0x40b22006, 0x5dd5, 0x42f2, { 0xbf, 0xe7, 0x7d, 0xbf, 0x7, 0x57, 0xab, 0x8b } }

static const nsModuleComponentInfo components[] =
{
  { "DOM View",
    IN_DOMVIEW_CID, 
    "@mozilla.org/inspector/dom-view;1",
    inDOMViewConstructor },

  { "Deep Tree Walker", 
    IN_DEEPTREEWALKER_CID, 
    "@mozilla.org/inspector/deep-tree-walker;1",
    inDeepTreeWalkerConstructor },

  { "Flasher", 
    IN_FLASHER_CID, 
    "@mozilla.org/inspector/flasher;1", 
    inFlasherConstructor },

  { "CSS Value Search", 
    IN_CSSVALUESEARCH_CID, 
    "@mozilla.org/inspector/search;1?type=cssvalue", 
    inCSSValueSearchConstructor },

  { "File Search", 
    IN_FILESEARCH_CID, 
    "@mozilla.org/inspector/search;1?type=file", 
    inFileSearchConstructor },

  { "DOM Utils", 
    IN_DOMUTILS_CID, 
    "@mozilla.org/inspector/dom-utils;1", 
    inDOMUtilsConstructor }
};

PR_STATIC_CALLBACK(nsresult)
InspectorModuleCtor(nsIModule* aSelf)
{
  inDOMView::InitAtoms();
  return NS_OK;
}

NS_IMPL_NSGETMODULE_WITH_CTOR(nsInspectorModule, components,
                              InspectorModuleCtor)
