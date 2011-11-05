/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXBLPrototypeResources_h__
#define nsXBLPrototypeResources_h__

#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsIStyleRuleProcessor.h"

class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsSupportsHashtable;
class nsXBLResourceLoader;
class nsXBLPrototypeBinding;
class nsCSSStyleSheet;

// *********************************************************************/
// The XBLPrototypeResources class

class nsXBLPrototypeResources
{
public:
  void LoadResources(bool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);
  nsresult FlushSkinSheets();

  nsresult Write(nsIObjectOutputStream* aStream);

  nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding);
  ~nsXBLPrototypeResources();

// MEMBER VARIABLES
  nsXBLResourceLoader* mLoader; // A loader object. Exists only long enough to load resources, and then it dies.
  typedef nsTArray<nsRefPtr<nsCSSStyleSheet> > sheet_array_type;
  sheet_array_type mStyleSheetList; // A list of loaded stylesheets for this binding.

  // The list of stylesheets converted to a rule processor.
  nsCOMPtr<nsIStyleRuleProcessor> mRuleProcessor;
};

#endif

