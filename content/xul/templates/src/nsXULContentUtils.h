/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Chris Waterson <waterson@netscape.com>
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
class nsICollation;

class nsXULContentUtils
{
protected:
    static nsIRDFService* gRDF;
    static nsIDateTimeFormat* gFormat;
    static nsICollation *gCollation;

    static PRBool gDisableXULCache;

    static int
    DisableXULCacheChangedCallback(const char* aPrefName, void* aClosure);

public:
    static nsresult
    Init();

    static nsresult
    Finish();

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
    GetTextForNode(nsIRDFNode* aNode, nsAString& aResult);

    /**
     * Construct a URI from the element ID given.  This uses aElement as the
     * ref and aDocument's document URI as the base.  If aDocument's document
     * URI does not support refs, this will throw NS_ERROR_NOT_AVAILABLE.
     */
    static nsresult
    MakeElementURI(nsIDocument* aDocument, const nsAString& aElementID, nsCString& aURI);

    static nsresult
    MakeElementResource(nsIDocument* aDocument, const nsAString& aElementID, nsIRDFResource** aResult);

    /**
     * Extract the element ID from aURI.  Note that aURI must be an absolute
     * URI string in UTF8; the element ID is the ref from the URI.  If the
     * scheme does not support refs, then the ID will be empty.
     */
    static nsresult
    MakeElementID(nsIDocument* aDocument, const nsACString& aURI, nsAString& aElementID);

    static nsresult
    GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult);

    static nsresult
    GetResource(PRInt32 aNameSpaceID, const nsAString& aAttribute, nsIRDFResource** aResult);

    static nsresult
    SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement);
    
    static nsIRDFService*
    RDFService()
    {
        return gRDF;
    }

    static nsICollation*
    GetCollation();

#define XUL_RESOURCE(ident, uri) static nsIRDFResource* ident
#define XUL_LITERAL(ident, val)  static nsIRDFLiteral*  ident
#include "nsXULResourceList.h"
#undef XUL_RESOURCE
#undef XUL_LITERAL
};

#endif // nsXULContentUtils_h__
