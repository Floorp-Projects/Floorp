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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peter@propagandism.org>
 *
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

#ifndef nsContentCreatorFunctions_h__
#define nsContentCreatorFunctions_h__

#include "nscore.h"
#include "nsCOMPtr.h"

/**
 * Functions to create content, to be used only inside Gecko
 * (mozilla/content and mozilla/layout).
 */

class nsAString;
class nsIContent;
class nsIDocument;
class nsINodeInfo;
class imgIRequest;
class nsNodeInfoManager;
class nsGenericHTMLElement;

nsresult
NS_NewElement(nsIContent** aResult, PRInt32 aElementType,
              nsINodeInfo* aNodeInfo, PRBool aFromParser);

nsresult
NS_NewXMLElement(nsIContent** aResult, nsINodeInfo* aNodeInfo);

/**
 * aNodeInfoManager must not be null.
 */
nsresult
NS_NewTextNode(nsIContent **aResult, nsNodeInfoManager *aNodeInfoManager);

/**
 * aNodeInfoManager must not be null.
 */
nsresult
NS_NewCommentNode(nsIContent **aResult, nsNodeInfoManager *aNodeInfoManager);

/**
 * aNodeInfoManager must not be null.
 */
nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               nsNodeInfoManager *aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData);

/**
 * aNodeInfoManager must not be null.
 */
nsresult
NS_NewXMLStylesheetProcessingInstruction(nsIContent** aInstancePtrResult,
                                         nsNodeInfoManager *aNodeInfoManager,
                                         const nsAString& aData);

/**
 * aNodeInfoManager must not be null.
 */
nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult,
                      nsNodeInfoManager *aNodeInfoManager);

nsresult
NS_NewHTMLElement(nsIContent** aResult, nsINodeInfo *aNodeInfo,
                  PRBool aFromParser);

// First argument should be nsHTMLTag, but that adds dependency to parser
// for a bunch of files.
already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(PRUint32 aNodeType, nsINodeInfo *aNodeInfo,
                  PRBool aFromParser);

#ifdef MOZ_MATHML
nsresult
NS_NewMathMLElement(nsIContent** aResult, nsINodeInfo* aNodeInfo);
#endif

#ifdef MOZ_XUL
nsresult
NS_NewXULElement(nsIContent** aResult, nsINodeInfo* aNodeInfo);
#endif

#ifdef MOZ_SVG
nsresult
NS_NewSVGElement(nsIContent** aResult, nsINodeInfo* aNodeInfo,
                 PRBool aFromParser);
#endif

nsresult
NS_NewGenConImageContent(nsIContent** aResult, nsINodeInfo* aNodeInfo,
                         imgIRequest* aImageRequest);

nsresult
NS_NewXMLEventsElement(nsIContent** aResult, nsINodeInfo* aNodeInfo);

#endif // nsContentCreatorFunctions_h__
