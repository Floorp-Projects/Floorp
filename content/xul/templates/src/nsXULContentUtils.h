/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Chris Waterson <waterson@netscape.com>
 *
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

/*

  A package of routines shared by the XUL content code.

 */

#ifndef nsXULContentUtils_h__
#define nsXULContentUtils_h__

#include "nsISupports.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsIDOMNodeList;
class nsIRDFNode;
class nsCString;
class nsString;
class nsIRDFResource;
class nsIRDFLiteral;
class nsIRDFService;
class nsINameSpaceManager;
class nsIDateTimeFormat;

class nsXULContentUtils
{
protected:
    static nsrefcnt gRefCnt;
    static nsIRDFService* gRDF;
    static nsINameSpaceManager* gNameSpaceManager;
    static nsIDateTimeFormat* gFormat;

    static PRBool gDisableXULCache;

    static int PR_CALLBACK
    DisableXULCacheChangedCallback(const char* aPrefName, void* aClosure);

public:
    static nsresult
    Init();

    static nsresult
    Finish();

    static nsresult
    AttachTextNode(nsIContent* parent, nsIRDFNode* value);
    
    static nsresult
    FindChildByTag(nsIContent *aElement,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aTag,
                   nsIContent **aResult);

    static nsresult
    FindChildByResource(nsIContent* aElement,
                        nsIRDFResource* aResource,
                        nsIContent** aResult);

    static nsresult
    GetElementResource(nsIContent* aElement, nsIRDFResource** aResult);

    static nsresult
    GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult);

    static nsresult
    GetTextForNode(nsIRDFNode* aNode, nsAWritableString& aResult);

    static nsresult
    GetElementLogString(nsIContent* aElement, nsAWritableString& aResult);

    static nsresult
    GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsAWritableString& aResult);

    static nsresult
    MakeElementURI(nsIDocument* aDocument, const nsAReadableString& aElementID, nsCString& aURI);

    static nsresult
    MakeElementResource(nsIDocument* aDocument, const nsAReadableString& aElementID, nsIRDFResource** aResult);

    static nsresult
    MakeElementID(nsIDocument* aDocument, const nsAReadableString& aURI, nsAWritableString& aElementID);

    static PRBool
    IsContainedBy(nsIContent* aElement, nsIContent* aContainer);

    static nsresult
    GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult);

    static nsresult
    GetResource(PRInt32 aNameSpaceID, const nsAReadableString& aAttribute, nsIRDFResource** aResult);

    static nsresult
    SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement);

#define XUL_RESOURCE(ident, uri) static nsIRDFResource* ident
#define XUL_LITERAL(ident, val)  static nsIRDFLiteral*  ident
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL
};


#endif // nsXULContentUtils_h__

