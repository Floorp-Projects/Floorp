#include "GeckoUtils.h"

#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMCharacterData.h"
#include "nsUnicharUtils.h"

#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIURI.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"

/* static */
void GeckoUtils::GatherTextUnder(nsIDOMNode* aNode, nsString& aResult) 
{
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
          nsCOMPtr<nsIDOMNode> nextSibling2;
          parentNode->GetNextSibling(getter_AddRefs(nextSibling2));
          node = nextSibling2;
          depth--;
        }
      }
    }
  }
  
  text.CompressWhitespace();
  aResult = text;
}

/* static */
void GeckoUtils::GetEnclosingLinkElementAndHref(nsIDOMNode* aNode, nsIDOMElement** aLinkContent, nsString& aHref)
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

/* static */
void GeckoUtils::GetURIForDocShell(nsIDocShell* aDocShell, nsACString& aURI)
{
  nsCOMPtr<nsIWebNavigation> nav = do_QueryInterface(aDocShell);
  nsCOMPtr<nsIURI> uri;
  nav->GetCurrentURI(getter_AddRefs(uri));
  if (!uri)
    aURI.Truncate();
  else
    uri->GetSpec(aURI);
}

// NOTE: this addrefs the result!
/* static */
void GeckoUtils::FindDocShellForURI (nsIURI *aURI, nsIDocShell *aRoot, nsIDocShell **outMatch)
{
  *outMatch = nsnull;
  if (!aURI || !aRoot)
    return;

  // get the URI we're looking for
  nsCAutoString soughtURI;
  aURI->GetSpec (soughtURI);

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  aRoot->GetDocShellEnumerator (nsIDocShellTreeItem::typeContent, 
                                nsIDocShell::ENUMERATE_FORWARDS, 
                                getter_AddRefs (docShellEnumerator));
  if (!docShellEnumerator)
    return;

  PRBool hasMore = PR_FALSE;
  nsCOMPtr<nsIDocShellTreeItem> curItem;

  while (NS_SUCCEEDED (docShellEnumerator->HasMoreElements (&hasMore)) && hasMore) {
    // get the next element
    nsCOMPtr<nsISupports> curSupports;
    docShellEnumerator->GetNext (getter_AddRefs (curSupports));
    if (!curSupports)
      return;

    nsCOMPtr<nsIDocShell> curDocShell = do_QueryInterface (curSupports);
    if (!curDocShell)
      return;

    // get this docshell's URI
    nsCAutoString docShellURI;
    GetURIForDocShell (curDocShell, docShellURI);

    if (docShellURI.Equals (soughtURI)) {
      // we found it!
      NS_ADDREF (*outMatch = curDocShell.get());
      return;
    }
  }

  // We never found the docshell.
}