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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef __GeckoUtils_h__
#define __GeckoUtils_h__

#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMCharacterData.h"
#include "nsUnicharUtils.h"

class GeckoUtils
{
public:
  static void GatherTextUnder(nsIDOMNode* aNode, nsString& aResult) {
    nsAutoString text;
    nsCOMPtr<nsIDOMNode> node;
    aNode->GetFirstChild(getter_AddRefs(node));
    PRUint32 depth = 1;
    while (node && depth) {
      nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(node));
      PRUint16 nodeType;
      node->GetNodeType(&nodeType);
      if (charData && nodeType == nsIDOMNode::TEXT_NODE) {
        // Add this text to our collection.
        text += NS_LITERAL_STRING(" ");
        nsAutoString data;
        charData->GetData(data);
        text += data;
      }
      else {
        nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(node));
        if (img) {
          nsAutoString altText;
          img->GetAlt(altText);
          if (!altText.IsEmpty()) {
            text = altText;
            break;
          }
        }
      }

      // Find the next node to test.
      PRBool hasChildNodes;
      node->HasChildNodes(&hasChildNodes);
      if (hasChildNodes) {
        nsCOMPtr<nsIDOMNode> temp = node;
        temp->GetFirstChild(getter_AddRefs(node));
        depth++;
      }
      else {
        nsCOMPtr<nsIDOMNode> nextSibling;
        node->GetNextSibling(getter_AddRefs(nextSibling));
        if (nextSibling)
          node = nextSibling;
        else {
          nsCOMPtr<nsIDOMNode> parentNode;
          node->GetParentNode(getter_AddRefs(parentNode));
          if (!parentNode)
            node = nsnull;
          else {
            nsCOMPtr<nsIDOMNode> nextSibling;
            parentNode->GetNextSibling(getter_AddRefs(nextSibling));
            node = nextSibling;
            depth--;
          }
        }
      }
    }

    text.CompressWhitespace();
    aResult = text;
  };

  static void GetEnclosingLinkElementAndHref(nsIDOMNode* aNode, nsIDOMElement** aLinkContent, nsString& aHref)
  {
    nsCOMPtr<nsIDOMElement> content(do_QueryInterface(aNode));
    nsAutoString localName;
    if (content)
      content->GetLocalName(localName);

    nsCOMPtr<nsIDOMElement> linkContent;
    ToLowerCase(localName);
    nsAutoString href;
    if (localName.Equals(NS_LITERAL_STRING("a")) ||
        localName.Equals(NS_LITERAL_STRING("area")) ||
        localName.Equals(NS_LITERAL_STRING("link"))) {
      PRBool hasAttr;
      content->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);
      if (hasAttr) {
        linkContent = content;
        nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(linkContent));
        if (anchor)
          anchor->GetHref(href);
        else {
          nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(linkContent));
          if (area)
            area->GetHref(href);
          else {
            nsCOMPtr<nsIDOMHTMLLinkElement> link(do_QueryInterface(linkContent));
            if (link)
              link->GetHref(href);
          }
        }
      }
    }
    else {
      nsCOMPtr<nsIDOMNode> curr = aNode;
      nsCOMPtr<nsIDOMNode> temp = curr;
      temp->GetParentNode(getter_AddRefs(curr));
      while (curr) {
        content = do_QueryInterface(curr);
        if (!content)
          break;
        content->GetLocalName(localName);
        ToLowerCase(localName);
        if (localName.Equals(NS_LITERAL_STRING("a"))) {
          PRBool hasAttr;
          content->HasAttribute(NS_LITERAL_STRING("href"), &hasAttr);
          if (hasAttr) {
            linkContent = content;
            nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(linkContent));
            if (anchor)
              anchor->GetHref(href);
          }
          else
            linkContent = nsnull; // Links can't be nested.
          break;
        }

        temp = curr;
        temp->GetParentNode(getter_AddRefs(curr));
      }
    }

    *aLinkContent = linkContent;
    NS_IF_ADDREF(*aLinkContent);

    aHref = href;
  }
};


#endif
