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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
 *   Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessible.h"
#include "nsAccessibleRelation.h"
#include "nsHyperTextAccessibleWrap.h"

#include "nsIAccessibleDocument.h"
#include "nsIAccessibleHyperText.h"
#include "nsAccessibleTreeWalker.h"

#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULLabelElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsPIDOMWindow.h"

#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIForm.h"
#include "nsIFormControl.h"

#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIViewManager.h"
#include "nsIDocShellTreeItem.h"

#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "prdtoa.h"
#include "nsIAtom.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIURI.h"
#include "nsITimer.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"

#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#include "nsIDOMCharacterData.h"
#endif

/**
 * nsAccessibleDOMStringList implementation
 */
nsAccessibleDOMStringList::nsAccessibleDOMStringList()
{
}

nsAccessibleDOMStringList::~nsAccessibleDOMStringList()
{
}

NS_IMPL_ISUPPORTS1(nsAccessibleDOMStringList, nsIDOMDOMStringList)

NS_IMETHODIMP
nsAccessibleDOMStringList::Item(PRUint32 aIndex, nsAString& aResult)
{
  if (aIndex >= (PRUint32)mNames.Count()) {
    SetDOMStringToNull(aResult);
  } else {
    mNames.StringAt(aIndex, aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleDOMStringList::GetLength(PRUint32 *aLength)
{
  *aLength = (PRUint32)mNames.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsAccessibleDOMStringList::Contains(const nsAString& aString, PRBool *aResult)
{
  *aResult = mNames.IndexOf(aString) > -1;

  return NS_OK;
}

/*
 * Class nsAccessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
NS_IMPL_ADDREF_INHERITED(nsAccessible, nsAccessNode)
NS_IMPL_RELEASE_INHERITED(nsAccessible, nsAccessNode)

#ifdef DEBUG_A11Y
/*
 * static
 * help method. to detect whether this accessible object implements
 * nsIAccessibleText, when it is text or has text child node.
 */
PRBool nsAccessible::IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible)
{
  PRBool foundText = PR_FALSE;

  nsCOMPtr<nsIAccessible> child, nextSibling;
  aAccessible->GetFirstChild(getter_AddRefs(child));
  while (child) {
    if (IsText(child)) {
      foundText = PR_TRUE;
      break;
    }
    child->GetNextSibling(getter_AddRefs(nextSibling));
    child.swap(nextSibling);
  }
  if (foundText) {
    // found text child node
    nsCOMPtr<nsIAccessibleText> text = do_QueryInterface(aAccessible);
    if (!text) {
      return PR_FALSE;
    }
  }
  return PR_TRUE; 
}
#endif

nsresult nsAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Custom-built QueryInterface() knows when we support nsIAccessibleSelectable
  // based on role attribute and waistate:multiselectable
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsIAccessible))) {
    *aInstancePtr = static_cast<nsIAccessible*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if(aIID.Equals(NS_GET_IID(nsPIAccessible))) {
    *aInstancePtr = static_cast<nsPIAccessible*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessibleSelectable))) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    if (!content) {
      return NS_ERROR_FAILURE; // This accessible has been shut down
    }
    if (HasRoleAttribute(content)) {
      // If we have an XHTML role attribute present and the
      // waistate multiselectable attribute not empty or false, then we need
      // to support nsIAccessibleSelectable
      // If either attribute (role or multiselectable) change, then we'll
      // destroy this accessible so that we can follow COM identity rules.
      static nsIContent::AttrValuesArray strings[] =
        {&nsAccessibilityAtoms::_empty, &nsAccessibilityAtoms::_false, nsnull};
      if (content->FindAttrValueIn(kNameSpaceID_WAIProperties ,
                                   nsAccessibilityAtoms::multiselectable,
                                   strings, eCaseMatters) ==
          nsIContent::ATTR_VALUE_NO_MATCH) {
        *aInstancePtr = static_cast<nsIAccessibleSelectable*>(this);
        NS_ADDREF_THIS();
        return NS_OK;
      }
    }
  }

  if (aIID.Equals(NS_GET_IID(nsIAccessibleValue))) {
    if (mRoleMapEntry && mRoleMapEntry->valueRule != eNoValue) {
      *aInstancePtr = static_cast<nsIAccessibleValue*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
  }                       

  if (aIID.Equals(NS_GET_IID(nsIAccessibleHyperLink))) {
    nsCOMPtr<nsIAccessible> parent(GetParent());
    nsCOMPtr<nsIAccessibleHyperText> hyperTextParent(do_QueryInterface(parent));
    if (hyperTextParent) {
      *aInstancePtr = static_cast<nsIAccessibleHyperLink*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
  }

  return nsAccessNodeWrap::QueryInterface(aIID, aInstancePtr);
}

nsAccessible::nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell): nsAccessNodeWrap(aNode, aShell), 
  mParent(nsnull), mFirstChild(nsnull), mNextSibling(nsnull), mRoleMapEntry(nsnull),
  mAccChildCount(eChildCountUninitialized)
{
#ifdef NS_DEBUG_X
   {
     nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aShell));
     printf(">>> %p Created Acc - DOM: %p  PS: %p", 
            (void*)static_cast<nsIAccessible*>(this), (void*)aNode,
            (void*)shell.get());
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      nsAutoString buf;
      if (content->NodeInfo())
        content->NodeInfo()->GetQualifiedName(buf);
      printf(" Con: %s@%p", NS_ConvertUTF16toUTF8(buf).get(), (void *)content.get());
      if (NS_SUCCEEDED(GetName(buf))) {
        printf(" Name:[%s]", NS_ConvertUTF16toUTF8(buf).get());
       }
     }
     printf("\n");
   }
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessible::~nsAccessible()
{
}

NS_IMETHODIMP nsAccessible::GetName(nsAString& aName)
{
  aName.Truncate();
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }

  PRBool canAggregateName = mRoleMapEntry &&
                            mRoleMapEntry->nameRule == eNameOkFromChildren;

  if (content->IsNodeOfType(nsINode::eHTML)) {
    return GetHTMLName(aName, canAggregateName);
  }

  if (content->IsNodeOfType(nsINode::eXUL)) {
    return GetXULName(aName, canAggregateName);
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetDescription(nsAString& aDescription)
{
  // There are 4 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. It has no DHTML describedby property
  // 3. it doesn't have an accName; or
  // 4. its title attribute already equals to its accName nsAutoString name; 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }
  if (!content->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString description;
    nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::describedby, description);
    if (NS_FAILED(rv)) {
      PRBool isXUL = content->IsNodeOfType(nsINode::eXUL);
      if (isXUL) {
        // Try XUL <description control="[id]">description text</description>
        nsIContent *descriptionContent =
          FindNeighbourPointingToNode(content, nsAccessibilityAtoms::description,
                                      nsAccessibilityAtoms::control);

        if (descriptionContent) {
          // We have a description content node
          AppendFlatStringFromSubtree(descriptionContent, &description);
        }
      }
      if (description.IsEmpty()) {
        nsIAtom *descAtom = isXUL ? nsAccessibilityAtoms::tooltiptext :
                                    nsAccessibilityAtoms::title;
        if (content->GetAttr(kNameSpaceID_None, descAtom, description)) {
          nsAutoString name;
          GetName(name);
          if (name.IsEmpty() || description == name) {
            // Don't use tooltip for a description if this object
            // has no name or the tooltip is the same as the name
            description.Truncate();
          }
        }
      }
    }
    description.CompressWhitespace();
    aDescription = description;
  }

  return NS_OK;
}

// mask values for ui.key.chromeAccess and ui.key.contentAccess
#define NS_MODIFIER_SHIFT    1
#define NS_MODIFIER_CONTROL  2
#define NS_MODIFIER_ALT      4
#define NS_MODIFIER_META     8

// returns the accesskey modifier mask used in the given node's context
// (i.e. chrome or content), or 0 if an error occurs
static PRInt32
GetAccessModifierMask(nsIContent* aContent)
{
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefBranch)
    return 0;

  // use ui.key.generalAccessKey (unless it is -1)
  PRInt32 accessKey;
  nsresult rv = prefBranch->GetIntPref("ui.key.generalAccessKey", &accessKey);
  if (NS_SUCCEEDED(rv) && accessKey != -1) {
    switch (accessKey) {
      case nsIDOMKeyEvent::DOM_VK_SHIFT:   return NS_MODIFIER_SHIFT;
      case nsIDOMKeyEvent::DOM_VK_CONTROL: return NS_MODIFIER_CONTROL;
      case nsIDOMKeyEvent::DOM_VK_ALT:     return NS_MODIFIER_ALT;
      case nsIDOMKeyEvent::DOM_VK_META:    return NS_MODIFIER_META;
      default:                             return 0;
    }
  }

  // get the docShell to this DOMNode, return 0 on failure
  nsCOMPtr<nsIDocument> document = aContent->GetCurrentDoc();
  if (!document)
    return 0;
  nsCOMPtr<nsISupports> container = document->GetContainer();
  if (!container)
    return 0;
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(container));
  if (!treeItem)
    return 0;

  // determine the access modifier used in this context
  PRInt32 itemType, accessModifierMask = 0;
  treeItem->GetItemType(&itemType);
  switch (itemType) {

  case nsIDocShellTreeItem::typeChrome:
    rv = prefBranch->GetIntPref("ui.key.chromeAccess", &accessModifierMask);
    break;

  case nsIDocShellTreeItem::typeContent:
    rv = prefBranch->GetIntPref("ui.key.contentAccess", &accessModifierMask);
    break;
  }

  return NS_SUCCEEDED(rv) ? accessModifierMask : 0;
}

NS_IMETHODIMP
nsAccessible::GetKeyboardShortcut(nsAString& aAccessKey)
{
  aAccessKey.Truncate();

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content)
    return NS_ERROR_FAILURE;

  PRUint32 key = nsAccUtils::GetAccessKeyFor(content);
  if (!key) {
    nsCOMPtr<nsIContent> labelContent(GetLabelContent(content));
    if (labelContent)
      key = nsAccUtils::GetAccessKeyFor(labelContent);
  }

  if (!key)
    return NS_OK;

  nsAutoString accesskey(key);

  // Append the modifiers in reverse order, result: Control+Alt+Shift+Meta+<key>
  nsAutoString propertyKey;
  PRInt32 modifierMask = GetAccessModifierMask(content);
  if (modifierMask & NS_MODIFIER_META) {
    propertyKey.AssignLiteral("VK_META");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_SHIFT) {
    propertyKey.AssignLiteral("VK_SHIFT");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_ALT) {
    propertyKey.AssignLiteral("VK_ALT");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }
  if (modifierMask & NS_MODIFIER_CONTROL) {
    propertyKey.AssignLiteral("VK_CONTROL");
    nsAccessible::GetFullKeyName(propertyKey, accesskey, accesskey);
  }

  aAccessKey = accesskey;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetParent(nsIAccessible *aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetFirstChild(nsIAccessible *aFirstChild)
{
#ifdef DEBUG
  // If there's parent of this child already, make sure it's us!
  nsCOMPtr<nsPIAccessible> privChild = do_QueryInterface(aFirstChild);
  if (privChild) {
    nsCOMPtr<nsIAccessible> parent;
    privChild->GetCachedParent(getter_AddRefs(parent));
    NS_ASSERTION(!parent || parent == this, "Stealing child!");
  }
#endif

  mFirstChild = aFirstChild;

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetNextSibling(nsIAccessible *aNextSibling)
{
  mNextSibling = aNextSibling? aNextSibling: DEAD_END_ACCESSIBLE;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::Init()
{
  nsIContent *content = GetRoleContent(mDOMNode);
  nsAutoString roleString;
  if (content && GetRoleAttribute(content, roleString)) {
    // QI to nsIDOM3Node causes some overhead. Unfortunately we need to do this each
    // time there is a role attribute, because the prefixe to namespace mappings
    // can change within any subtree via the xmlns attribute
    nsCOMPtr<nsIDOM3Node> dom3Node(do_QueryInterface(content));
    if (dom3Node) {
      nsAutoString prefix;
      NS_NAMED_LITERAL_STRING(kWAIRoles_Namespace, "http://www.w3.org/2005/01/wai-rdf/GUIRoleTaxonomy#");
      dom3Node->LookupPrefix(kWAIRoles_Namespace, prefix);
      if (prefix.IsEmpty()) {
        // In HTML we are hardcoded to allow the exact prefix "wairole:" to 
        // always indicate that we are using the WAI roles. This allows DHTML accessibility
        // to be used within HTML
        nsCOMPtr<nsIDOMNSDocument> doc(do_QueryInterface(content->GetDocument()));
        if (doc) {
          nsAutoString mimeType;
          doc->GetContentType(mimeType);
          if (mimeType.EqualsLiteral("text/html")) {
            prefix = NS_LITERAL_STRING("wairole");
          }
        }
      }
      prefix += ':';
      PRUint32 length = prefix.Length();
      if (length > 1 && StringBeginsWith(roleString, prefix)) {
        roleString.Cut(0, length);
        nsCString utf8Role = NS_ConvertUTF16toUTF8(roleString); // For easy comparison
        ToLowerCase(utf8Role);
        PRUint32 index;
        for (index = 0; nsARIAMap::gWAIRoleMap[index].roleString; index ++) {
          if (utf8Role.Equals(nsARIAMap::gWAIRoleMap[index].roleString)) {
            break; // The dynamic role attribute maps to an entry in our table
          }
        }
        // Always use some entry if there is a role string
        // If no match, we use the last entry which maps to ROLE_NOTHING
        mRoleMapEntry = &nsARIAMap::gWAIRoleMap[index];
      }
    }
  }

  return nsAccessNodeWrap::Init();
}

nsIContent *nsAccessible::GetRoleContent(nsIDOMNode *aDOMNode)
{
  // Given the DOM node for an acessible, return content node that
  // we should look at role string from
  // For non-document accessibles, this is the associated content node.
  // For doc accessibles, first try the <body> if it's HTML and there is
  // a role attribute used there.
  // For any other doc accessible , this is the document element.
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  if (!content) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aDOMNode));
    if (domDoc) {
      nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(aDOMNode));
      if (htmlDoc) {
        nsCOMPtr<nsIDOMHTMLElement> bodyElement;
        htmlDoc->GetBody(getter_AddRefs(bodyElement));
        content = do_QueryInterface(bodyElement);
      }
      if (!content || !HasRoleAttribute(content)) {
        nsCOMPtr<nsIDOMElement> docElement;
        domDoc->GetDocumentElement(getter_AddRefs(docElement));
        content = do_QueryInterface(docElement);
      }
    }
  }
  return content;
}

NS_IMETHODIMP nsAccessible::Shutdown()
{
  mNextSibling = nsnull;
  // Make sure none of it's children point to this parent
  if (mFirstChild) {
    nsCOMPtr<nsIAccessible> current(mFirstChild), next;
    while (current) {
      nsCOMPtr<nsPIAccessible> privateAcc(do_QueryInterface(current));
      current->GetNextSibling(getter_AddRefs(next));
      privateAcc->SetParent(nsnull);
      current = next;
    }
  }
  // Now invalidate the child count and pointers to other accessibles
  InvalidateChildren();
  if (mParent) {
    nsCOMPtr<nsPIAccessible> privateParent(do_QueryInterface(mParent));
    privateParent->InvalidateChildren();
    mParent = nsnull;
  }

  return nsAccessNodeWrap::Shutdown();
}

NS_IMETHODIMP nsAccessible::InvalidateChildren()
{
  // Document has transformed, reset our invalid children and child count

  // Reset the sibling pointers, they will be set up again the next time
  // CacheChildren() is called.
  // Note: we don't want to start creating accessibles at this point,
  // so don't use GetNextSibling() here. (bug 387252)
  nsAccessible* child = static_cast<nsAccessible*>(mFirstChild);
  while (child && child->mNextSibling != DEAD_END_ACCESSIBLE) {
    nsIAccessible *next = child->mNextSibling;
    child->mNextSibling = nsnull;
    child = static_cast<nsAccessible*>(next);
  }

  mAccChildCount = eChildCountUninitialized;
  mFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetParent(nsIAccessible **  aParent)
{
  nsresult rv = GetCachedParent(aParent);
  if (NS_FAILED(rv) || *aParent) {
    return rv;
  }

  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  NS_ENSURE_TRUE(docAccessible, NS_ERROR_FAILURE);

  return docAccessible->GetAccessibleInParentChain(mDOMNode, aParent);
}

NS_IMETHODIMP nsAccessible::GetCachedParent(nsIAccessible **  aParent)
{
  *aParent = nsnull;
  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

  /* readonly attribute nsIAccessible nextSibling; */
NS_IMETHODIMP nsAccessible::GetNextSibling(nsIAccessible * *aNextSibling) 
{ 
  *aNextSibling = nsnull; 
  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }
  if (!mParent) {
    nsCOMPtr<nsIAccessible> parent(GetParent());
    if (parent) {
      PRInt32 numChildren;
      parent->GetChildCount(&numChildren);  // Make sure we cache all of the children
    }
  }

  if (mNextSibling || !mParent) {
    // If no parent, don't try to calculate a new sibling
    // It either means we're at the root or shutting down the parent
    if (mNextSibling != DEAD_END_ACCESSIBLE) {
      NS_IF_ADDREF(*aNextSibling = mNextSibling);
    }
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

  /* readonly attribute nsIAccessible previousSibling; */
NS_IMETHODIMP nsAccessible::GetPreviousSibling(nsIAccessible * *aPreviousSibling) 
{
  *aPreviousSibling = nsnull;

  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> parent;
  if (NS_FAILED(GetParent(getter_AddRefs(parent))) || !parent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> testAccessible, prevSibling;
  parent->GetFirstChild(getter_AddRefs(testAccessible));
  while (testAccessible && this != testAccessible) {
    prevSibling = testAccessible;
    prevSibling->GetNextSibling(getter_AddRefs(testAccessible));
  }

  if (!prevSibling) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aPreviousSibling = prevSibling);
  return NS_OK;
}

  /* readonly attribute nsIAccessible firstChild; */
NS_IMETHODIMP nsAccessible::GetFirstChild(nsIAccessible * *aFirstChild) 
{  
  if (gIsCacheDisabled) {
    InvalidateChildren();
  }
  PRInt32 numChildren;
  GetChildCount(&numChildren);  // Make sure we cache all of the children

  NS_IF_ADDREF(*aFirstChild = mFirstChild);

  return NS_OK;  
}

  /* readonly attribute nsIAccessible lastChild; */
NS_IMETHODIMP nsAccessible::GetLastChild(nsIAccessible * *aLastChild)
{  
  GetChildAt(-1, aLastChild);
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetChildAt(PRInt32 aChildNum, nsIAccessible **aChild)
{
  // aChildNum is a zero-based index

  PRInt32 numChildren;
  GetChildCount(&numChildren);

  // If no children or aChildNum is larger than numChildren, return null
  if (aChildNum >= numChildren || numChildren == 0 || !mWeakShell) {
    *aChild = nsnull;
    return NS_ERROR_FAILURE;
  // If aChildNum is less than zero, set aChild to last index
  } else if (aChildNum < 0) {
    aChildNum = numChildren - 1;
  }

  nsCOMPtr<nsIAccessible> current(mFirstChild), nextSibling;
  PRInt32 index = 0;

  while (current) {
    nextSibling = current;
    if (++index > aChildNum) {
      break;
    }
    nextSibling->GetNextSibling(getter_AddRefs(current));
  }

  NS_IF_ADDREF(*aChild = nextSibling);

  return NS_OK;
}

// readonly attribute nsIArray children;
NS_IMETHODIMP nsAccessible::GetChildren(nsIArray **aOutChildren)
{
  nsCOMPtr<nsIMutableArray> children = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!children)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> curChild;
  while (NextChild(curChild)) {
    children->AppendElement(curChild, PR_FALSE);
  }
  
  NS_ADDREF(*aOutChildren = children);
  return NS_OK;
}

nsIAccessible *nsAccessible::NextChild(nsCOMPtr<nsIAccessible>& aAccessible)
{
  nsCOMPtr<nsIAccessible> nextChild;
  if (!aAccessible) {
    GetFirstChild(getter_AddRefs(nextChild));
  }
  else {
    aAccessible->GetNextSibling(getter_AddRefs(nextChild));
  }
  return (aAccessible = nextChild);
}

void nsAccessible::CacheChildren()
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    PRBool allowsAnonChildren = PR_FALSE;
    GetAllowsAnonChildAccessibles(&allowsAnonChildren);
    nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, allowsAnonChildren);
    // Seed the frame hint early while we're still on a container node.
    // This is better than doing the GetPrimaryFrameFor() later on
    // a text node, because text nodes aren't in the frame map.
    walker.mState.frame = GetFrame();

    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    PRInt32 childCount = 0;
    walker.GetFirstChild();
    SetFirstChild(walker.mState.accessible);

    while (walker.mState.accessible) {
      ++ childCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetNextSibling(walker.mState.accessible);
    }
    mAccChildCount = childCount;
  }
}

NS_IMETHODIMP nsAccessible::GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren)
{
  *aAllowsAnonChildren = PR_TRUE;
  return NS_OK;
}

/* readonly attribute long childCount; */
NS_IMETHODIMP nsAccessible::GetChildCount(PRInt32 *aAccChildCount) 
{
  CacheChildren();
  *aAccChildCount = mAccChildCount;
  return NS_OK;  
}

/* readonly attribute long indexInParent; */
NS_IMETHODIMP nsAccessible::GetIndexInParent(PRInt32 *aIndexInParent)
{
  *aIndexInParent = -1;
  if (!mWeakShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> parent;
  GetParent(getter_AddRefs(parent));
  if (!parent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> sibling;
  parent->GetFirstChild(getter_AddRefs(sibling));
  if (!sibling) {
    return NS_ERROR_FAILURE;
  }

  *aIndexInParent = 0;
  while (sibling != this) {
    NS_ASSERTION(sibling, "Never ran into the same child that we started from");

    if (!sibling)
      return NS_ERROR_FAILURE;

    ++*aIndexInParent;
    nsCOMPtr<nsIAccessible> tempAccessible;
    sibling->GetNextSibling(getter_AddRefs(tempAccessible));
    sibling = tempAccessible;
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::TestChildCache(nsIAccessible *aCachedChild)
{
#ifndef DEBUG_A11Y
  return NS_OK;
#else
  // All cached accessible nodes should be in the parent
  // It will assert if not all the children were created
  // when they were first cached, and no invalidation
  // ever corrected parent accessible's child cache.
  if (mAccChildCount == eChildCountUninitialized) {
    return NS_OK;
  }
  nsCOMPtr<nsIAccessible> sibling = mFirstChild;

  while (sibling != aCachedChild) {
    NS_ASSERTION(sibling, "[TestChildCache] Never ran into the same child that we started from");
    if (!sibling)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIAccessible> tempAccessible;
    sibling->GetNextSibling(getter_AddRefs(tempAccessible));
    sibling = tempAccessible;
  }
  return NS_OK;
#endif
}

nsresult nsAccessible::GetTranslatedString(const nsAString& aKey, nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  if (!gStringBundle || 
    NS_FAILED(gStringBundle->GetStringFromName(PromiseFlatString(aKey).get(), getter_Copies(xsValue)))) 
    return NS_ERROR_FAILURE;

  aStringOut.Assign(xsValue);
  return NS_OK;
}

nsresult nsAccessible::GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut)
{
  nsXPIDLString modifierName, separator;

  if (!gKeyStringBundle ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(aModifierName).get(), 
                                                    getter_Copies(modifierName))) ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(NS_LITERAL_STRING("MODIFIER_SEPARATOR")).get(), 
                                                    getter_Copies(separator)))) {
    return NS_ERROR_FAILURE;
  }

  aStringOut = modifierName + separator + aKeyName; 
  return NS_OK;
}

PRBool nsAccessible::IsVisible(PRBool *aIsOffscreen) 
{
  // We need to know if at least a kMinPixels around the object is visible
  // Otherwise it will be marked nsIAccessibleStates::STATE_OFFSCREEN
  // The STATE_INVISIBLE flag is for elements which are programmatically hidden
  
  *aIsOffscreen = PR_TRUE;

  const PRUint16 kMinPixels  = 12;
   // Set up the variables we need, return false if we can't get at them all
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  if (!shell) 
    return PR_FALSE;

  nsIViewManager* viewManager = shell->GetViewManager();
  if (!viewManager)
    return PR_FALSE;

  nsIFrame *frame = GetFrame();
  if (!frame) {
    return PR_FALSE;
  }

  // If visibility:hidden or visibility:collapsed then mark with STATE_INVISIBLE
  if (!frame->GetStyleVisibility()->IsVisible())
  {
      return PR_FALSE;
  }

  nsPresContext *presContext = shell->GetPresContext();
  if (!presContext)
    return PR_FALSE;

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate GetBoundsRect, because that is more expensive
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough
  // indicator

  nsRect relFrameRect = frame->GetRect();
  nsIView *containingView = frame->GetViewExternal();
  if (containingView) {
    // When frame itself has a view, it has the same bounds as the view
    relFrameRect.x = relFrameRect.y = 0;
  }
  else {
    nsPoint frameOffset;
    frame->GetOffsetFromView(frameOffset, &containingView);
    if (!containingView)
      return PR_FALSE;  // no view -- not visible
    relFrameRect.x = frameOffset.x;
    relFrameRect.y = frameOffset.y;
  }

  nsRectVisibility rectVisibility;
  viewManager->GetRectVisibility(containingView, relFrameRect,
                                 nsPresContext::CSSPixelsToAppUnits(kMinPixels),
                                 &rectVisibility);

  if (rectVisibility == nsRectVisibility_kZeroAreaRect) {
    if (frame->GetNextContinuation()) {
      // Zero area rects can occur in the first frame of a multi-frame text flow,
      // in which case the next frame exists because the text flow is visible
      rectVisibility = nsRectVisibility_kVisible;
    }
    else if (IsCorrectFrameType(frame, nsAccessibilityAtoms::inlineFrame)) {
      // Yuck. Unfortunately inline frames can contain larger frames inside of them,
      // so we can't really believe this is a zero area rect without checking more deeply.
      // GetBounds() will do that for us.
      PRInt32 x, y, width, height;
      GetBounds(&x, &y, &width, &height);
      if (width > 0 && height > 0) {
        rectVisibility = nsRectVisibility_kVisible;    
      }
    }
  }

  if (rectVisibility == nsRectVisibility_kZeroAreaRect || !mDOMNode) {
    return PR_FALSE;   // Hidden element
  }
  
  // Currently one of:
  // nsRectVisibility_kVisible, 
  // nsRectVisibility_kAboveViewport, 
  // nsRectVisibility_kBelowViewport, 
  // nsRectVisibility_kLeftOfViewport, 
  // nsRectVisibility_kRightOfViewport
  // This view says it is visible, but we need to check the parent view chain :(
  nsCOMPtr<nsIDOMDocument> domDoc;
  mDOMNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)  {
    return PR_FALSE;
  }

  PRBool isVisible = CheckVisibilityInParentChain(doc, containingView);
  if (isVisible && rectVisibility == nsRectVisibility_kVisible) {
    *aIsOffscreen = PR_FALSE;
  }
  return isVisible;
}

nsresult
nsAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = 0;

  if (aExtraState)
    *aExtraState = 0;

  if (!mDOMNode && aExtraState) {
    *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;
    return NS_OK; // Node shut down
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_OK;  // On document, this is not an error
  }

  // Set STATE_UNAVAILABLE state based on disabled attribute
  // The disabled attribute is mostly used in XUL elements and HTML forms, but
  // if someone sets it on another attribute, 
  // it seems reasonable to consider it unavailable
  PRBool isDisabled;
  if (content->IsNodeOfType(nsINode::eHTML)) {
    // In HTML, just the presence of the disabled attribute means it is disabled,
    // therefore disabled="false" indicates disabled!
    isDisabled = content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::disabled);
  }
  else {
    isDisabled = content->AttrValueIs(kNameSpaceID_None,
                                      nsAccessibilityAtoms::disabled,
                                      nsAccessibilityAtoms::_true,
                                      eCaseMatters);
  }
  if (isDisabled) {
    *aState |= nsIAccessibleStates::STATE_UNAVAILABLE;
  }
  else if (content->IsNodeOfType(nsINode::eELEMENT)) {
    nsIFrame *frame = GetFrame();
    if (frame && frame->IsFocusable()) {
      *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
    }

    if (gLastFocusedNode == mDOMNode) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  // Check if nsIAccessibleStates::STATE_INVISIBLE and
  // STATE_OFFSCREEN flags should be turned on for this object.
  PRBool isOffscreen;
  if (!IsVisible(&isOffscreen)) {
    *aState |= nsIAccessibleStates::STATE_INVISIBLE;
  }
  if (isOffscreen) {
    *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
  }

  return NS_OK;
}

  /* readonly attribute boolean focusedChild; */
NS_IMETHODIMP nsAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{ 
  nsCOMPtr<nsIAccessible> focusedChild;
  if (gLastFocusedNode == mDOMNode) {
    focusedChild = this;
  }
  else if (gLastFocusedNode) {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

    accService->GetAccessibleFor(gLastFocusedNode,
                                 getter_AddRefs(focusedChild));
    if (focusedChild) {
      nsCOMPtr<nsIAccessible> focusedParentAccessible;
      focusedChild->GetParent(getter_AddRefs(focusedParentAccessible));
      if (focusedParentAccessible != this) {
        focusedChild = nsnull;
      }
    }
  }

  NS_IF_ADDREF(*aFocusedChild = focusedChild);
  return NS_OK;
}

  /* nsIAccessible getChildAtPoint (in long x, in long y); */
NS_IMETHODIMP
nsAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                              nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  // Search an accessible at the given point starting from accessible document
  // because containing block (see CSS2) for out of flow element (for example,
  // absolutely positioned element) may be different from its DOM parent and
  // therefore accessible for containing block may be different from accessible
  // for DOM parent but GetFrameForPoint() should be called for containing block
  // to get an out of flow element.
  nsCOMPtr<nsIAccessibleDocument> accDocument;
  nsresult rv = GetAccessibleDocument(getter_AddRefs(accDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!accDocument)
    return NS_OK;

  nsCOMPtr<nsPIAccessNode> accessNodeDocument(do_QueryInterface(accDocument));
  NS_ASSERTION(accessNodeDocument,
               "nsIAccessibleDocument doesn't implement nsPIAccessNode");

  nsIFrame *frame = accessNodeDocument->GetFrame();
  NS_ENSURE_STATE(frame);

  nsPresContext *presContext = frame->PresContext();

  nsIntRect screenRect = frame->GetScreenRectExternal();
  nsPoint offset(presContext->DevPixelsToAppUnits(aX - screenRect.x),
                 presContext->DevPixelsToAppUnits(aY - screenRect.y));

  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
  nsIFrame *foundFrame = presShell->GetFrameForPoint(frame, offset);
  if (!foundFrame)
    return NS_OK;

  nsCOMPtr<nsIContent> content(foundFrame->GetContent());
  if (!content)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  nsCOMPtr<nsIAccessibilityService> accService = GetAccService();

  nsCOMPtr<nsIDOMNode> relevantNode;
  accService->GetRelevantContentNodeFor(node, getter_AddRefs(relevantNode));
  if (!relevantNode)
    return NS_OK;

  nsCOMPtr<nsIAccessible> accessible;
  accService->GetAccessibleFor(relevantNode, getter_AddRefs(accessible));
  if (!accessible)
    return NS_OK;

  nsCOMPtr<nsIAccessible> parent;
  accessible->GetParent(getter_AddRefs(parent));

  while (parent && parent != this) {
    accessible.swap(parent);
    accessible->GetParent(getter_AddRefs(parent));
  }

  if (parent)
    NS_ADDREF(*aAccessible = accessible);

  return NS_OK;
}

void nsAccessible::GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame)
{
/*
 * This method is used to determine the bounds of a content node.
 * Because HTML wraps and links are not always rectangular, this
 * method uses the following algorithm:
 *
 * 1) Start with an empty rectangle
 * 2) Add the rect for the primary frame from for the DOM node.
 * 3) For each next frame at the same depth with the same DOM node, add that rect to total
 * 4) If that frame is an inline frame, search deeper at that point in the tree, adding all rects
 */

  // Initialization area
  *aBoundingFrame = nsnull;
  nsIFrame *firstFrame = GetBoundsFrame();
  if (!firstFrame)
    return;

  // Find common relative parent
  // This is an ancestor frame that will incompass all frames for this content node.
  // We need the relative parent so we can get absolute screen coordinates
  nsIFrame *ancestorFrame = firstFrame;

  while (ancestorFrame) {  
    *aBoundingFrame = ancestorFrame;
    // If any other frame type, we only need to deal with the primary frame
    // Otherwise, there may be more frames attached to the same content node
    if (!IsCorrectFrameType(ancestorFrame, nsAccessibilityAtoms::inlineFrame) &&
        !IsCorrectFrameType(ancestorFrame, nsAccessibilityAtoms::textFrame))
      break;
    ancestorFrame = ancestorFrame->GetParent();
  }

  nsIFrame *iterFrame = firstFrame;
  nsCOMPtr<nsIContent> firstContent(do_QueryInterface(mDOMNode));
  nsIContent* iterContent = firstContent;
  PRInt32 depth = 0;

  // Look only at frames below this depth, or at this depth (if we're still on the content node we started with)
  while (iterContent == firstContent || depth > 0) {
    // Coordinates will come back relative to parent frame
    nsRect currFrameBounds = iterFrame->GetRect();
    
    // Make this frame's bounds relative to common parent frame
    currFrameBounds +=
      iterFrame->GetParent()->GetOffsetToExternal(*aBoundingFrame);

    // Add this frame's bounds to total
    aTotalBounds.UnionRect(aTotalBounds, currFrameBounds);

    nsIFrame *iterNextFrame = nsnull;

    if (IsCorrectFrameType(iterFrame, nsAccessibilityAtoms::inlineFrame)) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterNextFrame = iterFrame->GetFirstChild(nsnull);
    }

    if (iterNextFrame) 
      ++depth;  // Child was found in code above this: We are going deeper in this iteration of the loop
    else {  
      // Use next sibling if it exists, or go back up the tree to get the first next-in-flow or next-sibling 
      // within our search
      while (iterFrame) {
        iterNextFrame = iterFrame->GetNextContinuation();
        if (!iterNextFrame)
          iterNextFrame = iterFrame->GetNextSibling();
        if (iterNextFrame || --depth < 0) 
          break;
        iterFrame = iterFrame->GetParent();
      }
    }

    // Get ready for the next round of our loop
    iterFrame = iterNextFrame;
    if (iterFrame == nsnull)
      break;
    iterContent = nsnull;
    if (depth == 0)
      iterContent = iterFrame->GetContent();
  }
}


/* void getBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  // This routine will get the entire rectange for all the frames in this node
  // -------------------------------------------------------------------------
  //      Primary Frame for node
  //  Another frame, same node                <- Example
  //  Another frame, same node

  nsPresContext *presContext = GetPresContext();
  if (!presContext)
  {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  nsRect unionRectTwips;
  nsIFrame* aBoundingFrame = nsnull;
  GetBoundsRect(unionRectTwips, &aBoundingFrame);   // Unions up all primary frames for this node and all siblings after it
  if (!aBoundingFrame) {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  *x      = presContext->AppUnitsToDevPixels(unionRectTwips.x); 
  *y      = presContext->AppUnitsToDevPixels(unionRectTwips.y);
  *width  = presContext->AppUnitsToDevPixels(unionRectTwips.width);
  *height = presContext->AppUnitsToDevPixels(unionRectTwips.height);

  // We have the union of the rectangle, now we need to put it in absolute screen coords

  nsRect orgRectPixels = aBoundingFrame->GetScreenRectExternal();
  *x += orgRectPixels.x;
  *y += orgRectPixels.y;

  return NS_OK;
}

// helpers

/**
  * Static
  * Helper method to help sub classes make sure they have the proper
  *     frame when walking the frame tree to get at children and such
  */
PRBool nsAccessible::IsCorrectFrameType( nsIFrame* aFrame, nsIAtom* aAtom ) 
{
  NS_ASSERTION(aFrame != nsnull, "aFrame is null in call to IsCorrectFrameType!");
  NS_ASSERTION(aAtom != nsnull, "aAtom is null in call to IsCorrectFrameType!");

  return aFrame->GetType() == aAtom;
}


nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

already_AddRefed<nsIAccessible>
nsAccessible::GetMultiSelectFor(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, nsnull);
  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ENSURE_TRUE(accService, nsnull);
  nsCOMPtr<nsIAccessible> accessible;
  accService->GetAccessibleFor(aNode, getter_AddRefs(accessible));
  if (!accessible) {
    return nsnull;
  }

  PRUint32 state = State(accessible);
  if (0 == (state & nsIAccessibleStates::STATE_SELECTABLE)) {
    return nsnull;
  }

  PRUint32 containerRole;
  while (0 == (state & nsIAccessibleStates::STATE_MULTISELECTABLE)) {
    nsIAccessible *current = accessible;
    current->GetParent(getter_AddRefs(accessible));
    if (!accessible || (NS_SUCCEEDED(accessible->GetFinalRole(&containerRole)) &&
                        containerRole == nsIAccessibleRole::ROLE_PANE)) {
      return nsnull;
    }
    state = State(accessible);
  }
  nsIAccessible *returnAccessible = nsnull;
  accessible.swap(returnAccessible);
  return returnAccessible;
}

/* void removeSelection (); */
NS_IMETHODIMP nsAccessible::SetSelected(PRBool aSelect)
{
  // Add or remove selection
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 state = State(this);
  if (state & nsIAccessibleStates::STATE_SELECTABLE) {
    nsCOMPtr<nsIAccessible> multiSelect = GetMultiSelectFor(mDOMNode);
    if (!multiSelect) {
      return aSelect ? TakeFocus() : NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    NS_ASSERTION(content, "Called for dead accessible");

    // For DHTML widgets use WAI namespace
    PRUint32 nameSpaceID = mRoleMapEntry ? kNameSpaceID_WAIProperties : kNameSpaceID_None;
    if (aSelect) {
      return content->SetAttr(nameSpaceID, nsAccessibilityAtoms::selected, NS_LITERAL_STRING("true"), PR_TRUE);
    }
    return content->UnsetAttr(nameSpaceID, nsAccessibilityAtoms::selected, PR_TRUE);
  }

  return NS_ERROR_FAILURE;
}

/* void takeSelection (); */
NS_IMETHODIMP nsAccessible::TakeSelection()
{
  // Select only this item
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 state = State(this);
  if (state & nsIAccessibleStates::STATE_SELECTABLE) {
    nsCOMPtr<nsIAccessible> multiSelect = GetMultiSelectFor(mDOMNode);
    if (multiSelect) {
      nsCOMPtr<nsIAccessibleSelectable> selectable = do_QueryInterface(multiSelect);
      selectable->ClearSelection();
    }
    return SetSelected(PR_TRUE);
  }

  return NS_ERROR_FAILURE;
}

/* void takeFocus (); */
NS_IMETHODIMP nsAccessible::TakeFocus()
{ 
  nsCOMPtr<nsIDOMNSHTMLElement> htmlElement(do_QueryInterface(mDOMNode));
  if (htmlElement) {
    // HTML Elements also set the caret position
    // in order to affect tabbing order
    return htmlElement->Focus();
  }
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;
  }
  content->SetFocus(GetPresContext());

  return NS_OK;
}

nsresult nsAccessible::AppendStringWithSpaces(nsAString *aFlatString, const nsAString& textEquivalent)
{
  // Insert spaces to insure that words from controls aren't jammed together
  if (!textEquivalent.IsEmpty()) {
    if (!aFlatString->IsEmpty())
      aFlatString->Append(PRUnichar(' '));
    aFlatString->Append(textEquivalent);
    aFlatString->Append(PRUnichar(' '));
  }
  return NS_OK;
}

nsresult nsAccessible::AppendNameFromAccessibleFor(nsIContent *aContent,
                                                   nsAString *aFlatString,
                                                   PRBool aFromValue)
{
  nsAutoString textEquivalent, value;

  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(aContent));
  nsCOMPtr<nsIAccessible> accessible;
  if (domNode == mDOMNode) {
    accessible = this;
    if (!aFromValue) {
      // prevent recursive call GetName()
      return NS_OK;
    }
  }
  else {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
    accService->GetAccessibleInWeakShell(domNode, mWeakShell, getter_AddRefs(accessible));
  }
  if (accessible) {
    if (aFromValue) {
      accessible->GetValue(textEquivalent);
    }
    else {
      accessible->GetName(textEquivalent);
    }
  }

  textEquivalent.CompressWhitespace();
  return AppendStringWithSpaces(aFlatString, textEquivalent);
}

/*
 * AppendFlatStringFromContentNode and AppendFlatStringFromSubtree
 *
 * This method will glean useful text, in whatever form it exists, from any content node given to it.
 * It is used by any decendant of nsAccessible that needs to get text from a single node, as
 * well as by nsAccessible::AppendFlatStringFromSubtree, which gleans and concatenates text from any node and
 * that node's decendants.
 */

nsresult nsAccessible::AppendFlatStringFromContentNode(nsIContent *aContent, nsAString *aFlatString)
{
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    // If it's a text node, append the text
    PRBool isHTMLBlock = PR_FALSE;
    nsCOMPtr<nsIPresShell> shell = GetPresShell();
    if (!shell) {
      return NS_ERROR_FAILURE;  
    }

    nsIContent *parentContent = aContent->GetParent();
    nsCOMPtr<nsIContent> appendedSubtreeStart(do_QueryInterface(mDOMNode));
    if (parentContent && parentContent != appendedSubtreeStart) {
      nsIFrame *frame = shell->GetPrimaryFrameFor(parentContent);
      if (frame) {
        // If this text is inside a block level frame (as opposed to span level), we need to add spaces around that 
        // block's text, so we don't get words jammed together in final name
        // Extra spaces will be trimmed out later
        const nsStyleDisplay* display = frame->GetStyleDisplay();
        if (display->IsBlockOutside() ||
          display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
          isHTMLBlock = PR_TRUE;
          if (!aFlatString->IsEmpty()) {
            aFlatString->Append(PRUnichar(' '));
          }
        }
      }
    }
    if (aContent->TextLength() > 0) {
      nsIFrame *frame = shell->GetPrimaryFrameFor(aContent);
      NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
      nsresult rv = frame->GetRenderedText(aFlatString);
      NS_ENSURE_SUCCESS(rv, rv);
      if (isHTMLBlock && !aFlatString->IsEmpty()) {
        aFlatString->Append(PRUnichar(' '));
      }
    }
    return NS_OK;
  }

  nsAutoString textEquivalent;
  if (!aContent->IsNodeOfType(nsINode::eHTML)) {
    if (aContent->IsNodeOfType(nsINode::eXUL)) {
      nsCOMPtr<nsIPresShell> shell = GetPresShell();
      if (!shell) {
        return NS_ERROR_FAILURE;  
      }
      nsIFrame *frame = shell->GetPrimaryFrameFor(aContent);
      if (!frame || !frame->GetStyleVisibility()->IsVisible()) {
        return NS_OK;
      }

      nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl(do_QueryInterface(aContent));
      if (labeledEl) {
        labeledEl->GetLabel(textEquivalent);
      }
      else {
        if (aContent->NodeInfo()->Equals(nsAccessibilityAtoms::label, kNameSpaceID_XUL)) {
          aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value, textEquivalent);
        }
        if (textEquivalent.IsEmpty()) {
          aContent->GetAttr(kNameSpaceID_None,
                            nsAccessibilityAtoms::tooltiptext, textEquivalent);
        }
      }
      AppendNameFromAccessibleFor(aContent, &textEquivalent, PR_TRUE /* use value */);

      return AppendStringWithSpaces(aFlatString, textEquivalent);
    }
    return NS_OK; // Not HTML and not XUL -- we don't handle it yet
  }

  nsCOMPtr<nsIAtom> tag = aContent->Tag();
  if (tag == nsAccessibilityAtoms::img) {
    return AppendNameFromAccessibleFor(aContent, aFlatString);
  }

  if (tag == nsAccessibilityAtoms::input) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsAccessibilityAtoms::button, &nsAccessibilityAtoms::submit,
       &nsAccessibilityAtoms::reset, &nsAccessibilityAtoms::image, nsnull};
    if (aContent->FindAttrValueIn(kNameSpaceID_None, nsAccessibilityAtoms::type,
                                  strings, eIgnoreCase) >= 0) {
      return AppendNameFromAccessibleFor(aContent, aFlatString);
    }
  }

  if (tag == nsAccessibilityAtoms::object && !aContent->GetChildCount()) {
    // If object has no alternative content children, try title
    aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, textEquivalent);
  }
  else if (tag == nsAccessibilityAtoms::br) {
    // If it's a line break, insert a space so that words aren't jammed together
    aFlatString->AppendLiteral("\r\n");
    return NS_OK;
  }
  else if (tag != nsAccessibilityAtoms::a && tag != nsAccessibilityAtoms::area) { 
    AppendNameFromAccessibleFor(aContent, aFlatString, PR_TRUE /* use value */);
  }

  textEquivalent.CompressWhitespace();
  return AppendStringWithSpaces(aFlatString, textEquivalent);
}


nsresult nsAccessible::AppendFlatStringFromSubtree(nsIContent *aContent, nsAString *aFlatString)
{
  nsresult rv = AppendFlatStringFromSubtreeRecurse(aContent, aFlatString);
  if (NS_SUCCEEDED(rv) && !aFlatString->IsEmpty()) {
    nsAString::const_iterator start, end;
    aFlatString->BeginReading(start);
    aFlatString->EndReading(end);

    PRInt32 spacesToTruncate = 0;
    while (-- end != start && *end == ' ')
      ++ spacesToTruncate;

    if (spacesToTruncate > 0)
      aFlatString->Truncate(aFlatString->Length() - spacesToTruncate);
  }

  return rv;
}

nsresult nsAccessible::AppendFlatStringFromSubtreeRecurse(nsIContent *aContent, nsAString *aFlatString)
{
  // Depth first search for all text nodes that are decendants of content node.
  // Append all the text into one flat string
  PRUint32 numChildren = 0;
  nsCOMPtr<nsIDOMXULSelectControlElement> selectControlEl(do_QueryInterface(aContent));
  if (!selectControlEl) {  // Don't walk children of elements with options, just get label directly
    numChildren = aContent->GetChildCount();
  }

  if (numChildren == 0) {
    // There are no children or they are irrelvant: get the text from the current node
    AppendFlatStringFromContentNode(aContent, aFlatString);
    return NS_OK;
  }

  // There are relevant children: use them to get the text.
  PRUint32 index;
  for (index = 0; index < numChildren; index++) {
    AppendFlatStringFromSubtreeRecurse(aContent->GetChildAt(index), aFlatString);
  }
  return NS_OK;
}

nsIContent *nsAccessible::GetLabelContent(nsIContent *aForNode)
{
  if (aForNode->IsNodeOfType(nsINode::eXUL))
    return FindNeighbourPointingToNode(aForNode, nsAccessibilityAtoms::label,
                                       nsAccessibilityAtoms::control);

  return GetHTMLLabelContent(aForNode);
}

nsIContent* nsAccessible::GetHTMLLabelContent(nsIContent *aForNode)
{
  // Get either <label for="[id]"> element which explictly points to aForNode, or 
  // <label> ancestor which implicitly point to it
  nsIContent *walkUpContent = aForNode;

  // go up tree get name of ancestor label if there is one. Don't go up farther than form element
  while ((walkUpContent = walkUpContent->GetParent()) != nsnull) {
    nsIAtom *tag = walkUpContent->Tag();
    if (tag == nsAccessibilityAtoms::label) {
      return walkUpContent;  // An ancestor <label> implicitly points to us
    }
    if (tag == nsAccessibilityAtoms::form ||
        tag == nsAccessibilityAtoms::body) {
      // Reached top ancestor in form
      // There can be a label targeted at this control using the 
      // for="control_id" attribute. To save computing time, only 
      // look for those inside of a form element
      nsAutoString forId;
      aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, forId);
      // Actually we'll be walking down the content this time, with a depth first search
      if (forId.IsEmpty()) {
        break;
      }
      return FindDescendantPointingToID(&forId, walkUpContent,
                                        nsAccessibilityAtoms::_for);
    }
  }

  return nsnull;
}

nsresult nsAccessible::GetTextFromRelationID(nsIAtom *aIDAttrib, nsString &aName)
{
  // Get DHTML name from content subtree pointed to by ID attribute
  aName.Truncate();
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "Called from shutdown accessible");

  nsAutoString ids;
  if (!content->GetAttr(kNameSpaceID_WAIProperties, aIDAttrib, ids)) {
    return NS_ERROR_FAILURE;
  }
  ids.CompressWhitespace(PR_TRUE, PR_TRUE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  mDOMNode->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);
  
  nsresult rv = NS_ERROR_FAILURE;

  // Support idlist as in aaa::labelledby="id1 id2 id3"
  while (!ids.IsEmpty()) {
    nsAutoString id;
    PRInt32 idLength = ids.FindChar(' ');
    NS_ASSERTION(idLength != 0, "Should not be 0 because of CompressWhitespace() call above");
    if (idLength == kNotFound) {
      id = ids;
      ids.Truncate();
    } else {
      id = Substring(ids, 0, idLength);
      ids.Cut(0, idLength + 1);
    }

    if (!aName.IsEmpty()) {
      aName += ' '; // Need whitespace between multiple labels or descriptions
    }
    nsCOMPtr<nsIDOMElement> labelElement;
    domDoc->GetElementById(id, getter_AddRefs(labelElement));
    content = do_QueryInterface(labelElement);
    if (!content) {
      return NS_OK;
    }
    // We have a label content
    rv = AppendFlatStringFromSubtree(content, &aName);
    if (NS_SUCCEEDED(rv)) {
      aName.CompressWhitespace();
    }
  }
  
  return rv;
}

nsIContent*
nsAccessible::FindNeighbourPointingToNode(nsIContent *aForNode,
                                          nsIAtom *aTagName, nsIAtom *aRelationAttr,
                                          PRUint32 aRelationNameSpaceID,
                                          PRUint32 aAncestorLevelsToSearch)
{
  nsCOMPtr<nsIContent> binding;

  nsAutoString controlID;
  aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, controlID);
  if (controlID.IsEmpty()) {
    binding = aForNode->GetBindingParent();
    if (binding == aForNode)
      return nsnull;

    aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::anonid, controlID);
    if (controlID.IsEmpty())
      return nsnull;
  }

  // Look for label in subtrees of nearby ancestors
  PRUint32 count = 0;
  nsIContent *labelContent = nsnull;
  nsIContent *prevSearched = nsnull;

  while (!labelContent && ++count <= aAncestorLevelsToSearch &&
         (aForNode = aForNode->GetParent()) != nsnull) {

    if (aForNode == binding) {
      // When we reach the binding parent, make sure to check
      // all of its anonymous child subtrees
      nsCOMPtr<nsIDocument> doc = aForNode->GetCurrentDoc();
      nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(doc));
      if (!xblDoc)
        return nsnull;

      nsCOMPtr<nsIDOMNodeList> nodes;
      nsCOMPtr<nsIDOMElement> forElm(do_QueryInterface(aForNode));
      xblDoc->GetAnonymousNodes(forElm, getter_AddRefs(nodes));
      if (!nodes)
        return nsnull;

      PRUint32 length;
      nsresult rv = nodes->GetLength(&length);
      if (NS_FAILED(rv))
        return nsnull;

      for (PRUint32 index = 0; index < length && !labelContent; index++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = nodes->Item(index, getter_AddRefs(node));
        if (NS_FAILED(rv))
          return nsnull;

        nsCOMPtr<nsIContent> content = do_QueryInterface(node);
        if (!content)
          return nsnull;

        if (content != prevSearched) {
          labelContent = FindDescendantPointingToID(&controlID, content,  aRelationAttr,
                                                    aRelationNameSpaceID, nsnull,
                                                    aTagName);
        }
      }
      break;
    }

    labelContent = FindDescendantPointingToID(&controlID, aForNode,
                                              aRelationAttr, aRelationNameSpaceID,
                                              prevSearched, aTagName);
    prevSearched = aForNode;
  }

  return labelContent;
}

// Pass in aRelationAttr == nsnull if any <label> will do
nsIContent*
nsAccessible::FindDescendantPointingToID(const nsAString *aId,
                                         nsIContent *aLookContent,
                                         nsIAtom *aRelationAttr,
                                         PRUint32 aRelationNameSpaceID,
                                         nsIContent *aExcludeContent,
                                         nsIAtom *aTagType)
{
  if (!aTagType || aLookContent->Tag() == aTagType) {
    if (aRelationAttr) {
      // Check for ID in the attribute aRelationAttr, which can be a list
      nsAutoString idList;
      if (aLookContent->GetAttr(aRelationNameSpaceID, aRelationAttr, idList)) {
        idList.Insert(' ', 0);  // Surround idlist with spaces for search
        idList.Append(' ');
        nsAutoString id(*aId);
        id.Insert(' ', 0); // Surround id with spaces for search
        id.Append(' ');
        // idList is now a set of id's with spaces around each,
        // and id also has spaces around it.
        // If id is a substring of idList then we have a match
        if (idList.Find(id) != -1) {
          return aLookContent;
        }
      }
    }
    if (aTagType) {
      return nsnull;
    }
  }

  // Recursively search descendants for labels
  PRUint32 count  = 0;
  nsIContent *child;
  nsIContent *labelContent = nsnull;

  while ((child = aLookContent->GetChildAt(count++)) != nsnull) {
    if (child != aExcludeContent) {
      labelContent = FindDescendantPointingToID(aId, child, aRelationAttr,
                                                aRelationNameSpaceID, aExcludeContent,
                                                aTagType);
      if (labelContent) {
        return labelContent;
      }
    }
  }
  return nsnull;
}

/**
  * Only called if the element is not a nsIDOMXULControlElement. Initially walks up
  *   the DOM tree to the form, concatonating label elements as it goes. Then checks for
  *   labels with the for="controlID" property.
  */
nsresult nsAccessible::GetHTMLName(nsAString& aLabel, PRBool aCanAggregateSubtree)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;   // Node shut down
  }

  // Check for DHTML accessibility labelledby relationship property
  nsAutoString label;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::labelledby, label);
  if (NS_SUCCEEDED(rv)) {
    aLabel = label;
    return rv;
  }

  nsIContent *labelContent = GetHTMLLabelContent(content);
  if (labelContent) {
    AppendFlatStringFromSubtree(labelContent, &label);
    label.CompressWhitespace();
    if (!label.IsEmpty()) {
      aLabel = label;
      return NS_OK;
    }
  }

  if (aCanAggregateSubtree) {
    // Don't use AppendFlatStringFromSubtree for container widgets like menulist
    nsresult rv = AppendFlatStringFromSubtree(content, &aLabel);
    if (NS_SUCCEEDED(rv) && !aLabel.IsEmpty()) {
      return NS_OK;
    }
  }

  // Still try the title as as fallback method in that case.
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title,
                        aLabel)) {
    aLabel.SetIsVoid(PR_TRUE);
  }
  return NS_OK;
}

/**
  * 3 main cases for XUL Controls to be labeled
  *   1 - control contains label="foo"
  *   2 - control has, as a child, a label element
  *        - label has either value="foo" or children
  *   3 - non-child label contains control="controlID"
  *        - label has either value="foo" or children
  * Once a label is found, the search is discontinued, so a control
  *  that has a label child as well as having a label external to
  *  the control that uses the control="controlID" syntax will use
  *  the child label for its Name.
  */
nsresult nsAccessible::GetXULName(nsAString& aLabel, PRBool aCanAggregateSubtree)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "No nsIContent for DOM node");

  // First check for label override via accessibility labelledby relationship
  nsAutoString label;
  nsresult rv = GetTextFromRelationID(nsAccessibilityAtoms::labelledby, label);
  if (NS_SUCCEEDED(rv)) {
    aLabel = label;
    return rv;
  }

  // CASE #1 (via label attribute) -- great majority of the cases
  nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl(do_QueryInterface(mDOMNode));
  if (labeledEl) {
    rv = labeledEl->GetLabel(label);
  }
  else {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemEl(do_QueryInterface(mDOMNode));
    if (itemEl) {
      rv = itemEl->GetLabel(label);
    }
    else {
      nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mDOMNode));
      // Use label if this is not a select control element which 
      // uses label attribute to indicate which option is selected
      if (!select) {
        nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(mDOMNode));
        if (xulEl) {
          rv = xulEl->GetAttribute(NS_LITERAL_STRING("label"), label);
        }
      }
    }
  }

  // CASES #2 and #3 ------ label as a child or <label control="id" ... > </label>
  if (NS_FAILED(rv) || label.IsEmpty()) {
    label.Truncate();
    nsIContent *labelContent =
      FindNeighbourPointingToNode(content, nsAccessibilityAtoms::label,
                                  nsAccessibilityAtoms::control);

    nsCOMPtr<nsIDOMXULLabelElement> xulLabel(do_QueryInterface(labelContent));
    // Check if label's value attribute is used
    if (xulLabel && NS_SUCCEEDED(xulLabel->GetValue(label)) && label.IsEmpty()) {
      // If no value attribute, a non-empty label must contain
      // children that define it's text -- possibly using HTML
      AppendFlatStringFromSubtree(labelContent, &label);
    }
  }

  // XXX If CompressWhiteSpace worked on nsAString we could avoid a copy
  label.CompressWhitespace();
  if (!label.IsEmpty()) {
    aLabel = label;
    return NS_OK;
  }

  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::tooltiptext, label);
  label.CompressWhitespace();
  if (!label.IsEmpty()) {
    aLabel = label;
    return NS_OK;
  }

  // Can get text from title of <toolbaritem> if we're a child of a <toolbaritem>
  nsIContent *bindingParent = content->GetBindingParent();
  nsIContent *parent = bindingParent? bindingParent->GetParent() :
                                      content->GetParent();
  while (parent) {
    if (parent->Tag() == nsAccessibilityAtoms::toolbaritem &&
        parent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, label)) {
      label.CompressWhitespace();
      aLabel = label;
      return NS_OK;
    }
    parent = parent->GetParent();
  }

  // Don't use AppendFlatStringFromSubtree for container widgets like menulist
  return aCanAggregateSubtree? AppendFlatStringFromSubtree(content, &aLabel) : NS_OK;
}

PRBool nsAccessible::IsNodeRelevant(nsIDOMNode *aNode)
{
  // Can this node be accessible and attached to
  // the document's accessible tree?
  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ENSURE_TRUE(accService, PR_FALSE);
  nsCOMPtr<nsIDOMNode> relevantNode;
  accService->GetRelevantContentNodeFor(aNode, getter_AddRefs(relevantNode));
  return aNode == relevantNode;
}

NS_IMETHODIMP
nsAccessible::FireToolkitEvent(PRUint32 aEvent, nsIAccessible *aTarget,
                               void * aData)
{
  // Don't fire event for accessible that has been shut down.
  if (!mWeakShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleEvent> accEvent =
    new nsAccEvent(aEvent, aTarget, aData);
  NS_ENSURE_TRUE(accEvent, NS_ERROR_OUT_OF_MEMORY);

  return FireAccessibleEvent(accEvent);
}

NS_IMETHODIMP
nsAccessible::FireAccessibleEvent(nsIAccessibleEvent *aEvent)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  nsCOMPtr<nsIDOMNode> eventNode;
  aEvent->GetDOMNode(getter_AddRefs(eventNode));
  NS_ENSURE_TRUE(IsNodeRelevant(eventNode), NS_ERROR_FAILURE);

  nsCOMPtr<nsIObserverService> obsService =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(obsService, NS_ERROR_FAILURE);

  return obsService->NotifyObservers(aEvent, NS_ACCESSIBLE_EVENT_TOPIC, nsnull);
}

NS_IMETHODIMP nsAccessible::GetFinalRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (mRoleMapEntry) {
    *aRole = mRoleMapEntry->role;

    // These unfortunate exceptions don't fit into the ARIA table
    // This is where the nsIAccessible role depends on both the role and ARIA state
    if (*aRole == nsIAccessibleRole::ROLE_ENTRY) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
      if (content && 
          content->AttrValueIs(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::secret,
                               nsAccessibilityAtoms::_true, eCaseMatters)) {
        // For entry field with aaa:secret="true"
        *aRole = nsIAccessibleRole::ROLE_PASSWORD_TEXT;
      }
    }
    else if (*aRole == nsIAccessibleRole::ROLE_PUSHBUTTON) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
      if (content) {
        if (content->HasAttr(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::pressed)) {
          // For aaa:pressed="false" or aaa:pressed="true"
          // For simplicity, any pressed attribute indicates it's a toggle button
          *aRole = nsIAccessibleRole::ROLE_TOGGLE_BUTTON;
        }
        else if (content->AttrValueIs(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::haspopup,
                                      nsAccessibilityAtoms::_true, eCaseMatters)) {
          // For button with aaa:haspopup="true"
          *aRole = nsIAccessibleRole::ROLE_BUTTONMENU;
        }
      }
    }
  
    if (*aRole != nsIAccessibleRole::ROLE_NOTHING) {
      return NS_OK;
    }
  }
  return mDOMNode ? GetRole(aRole) : NS_ERROR_FAILURE;  // Node already shut down
}

NS_IMETHODIMP
nsAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attributes =
     do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
  NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);

  nsAccEvent::GetLastEventAttributes(mDOMNode, attributes);
 
  nsresult rv = GetAttributesInternal(attributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content = GetRoleContent(mDOMNode);
  if (content) {
    nsAutoString id;
    nsAutoString oldValueUnused;
    if (content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, id)) {
      attributes->SetStringProperty(NS_LITERAL_CSTRING("id"), id, oldValueUnused);    
    }
    // XXX In the future we may need to expose the dynamic content role inheritance chain
    // through this attribute
    nsAutoString xmlRole;
    if (GetRoleAttribute(content, xmlRole)) {
      attributes->SetStringProperty(NS_LITERAL_CSTRING("xml-roles"), xmlRole, oldValueUnused);          
    }

    char *ariaProperties[] = { "live", "channel", "atomic", "relevant", "datatype", "level",
                               "posinset", "setsize", "sort", "grab", "dropeffect"};

    for (PRUint32 index = 0; index < NS_ARRAY_LENGTH(ariaProperties); index ++) {
      nsAutoString value;
      nsCOMPtr<nsIAtom> attr = do_GetAtom(ariaProperties[index]);
      NS_ENSURE_TRUE(attr, NS_ERROR_OUT_OF_MEMORY);
      if (content->GetAttr(kNameSpaceID_WAIProperties, attr, value)) {
        ToLowerCase(value);
        attributes->SetStringProperty(nsDependentCString(ariaProperties[index]), value, oldValueUnused);    
      }
    }
  }

  if (!nsAccUtils::HasAccGroupAttrs(attributes)) {
    // The role of an accessible can be pointed by ARIA attribute but ARIA
    // posinset, level, setsize may be skipped. Therefore we calculate here
    // these properties to map them into description.

    // If accessible is invisible we don't want to calculate group ARIA
    // attributes for it.
    PRUint32 role = Role(this);
    if ((role == nsIAccessibleRole::ROLE_LISTITEM ||
        role == nsIAccessibleRole::ROLE_MENUITEM ||
        role == nsIAccessibleRole::ROLE_RADIOBUTTON ||
        role == nsIAccessibleRole::ROLE_PAGETAB ||
        role == nsIAccessibleRole::ROLE_OUTLINEITEM) &&
        0 == (State(this) & nsIAccessibleStates::STATE_INVISIBLE)) {
      nsCOMPtr<nsIAccessible> parent = GetParent();
      NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

      PRInt32 positionInGroup = 0;
      PRInt32 setSize = 0;

      nsCOMPtr<nsIAccessible> sibling, nextSibling;
      parent->GetFirstChild(getter_AddRefs(sibling));
      NS_ENSURE_TRUE(sibling, NS_ERROR_FAILURE);

      PRBool foundCurrent = PR_FALSE;
      PRUint32 siblingRole;
      while (sibling) {
        sibling->GetFinalRole(&siblingRole);
        if (siblingRole == role &&
            !(State(sibling) & nsIAccessibleStates::STATE_INVISIBLE)) {
          ++ setSize;
          if (!foundCurrent) {
            ++ positionInGroup;
            if (sibling == this)
              foundCurrent = PR_TRUE;
          }
        }
        sibling->GetNextSibling(getter_AddRefs(nextSibling));
        sibling = nextSibling;
      }

      PRInt32 groupLevel = 0;
      if (role == nsIAccessibleRole::ROLE_OUTLINEITEM) {
        groupLevel = 1;
        nsCOMPtr<nsIAccessible> nextParent;
        while (parent) {
          parent->GetFinalRole(&role);

          if (role == nsIAccessibleRole::ROLE_OUTLINE)
            break;
          if (role == nsIAccessibleRole::ROLE_GROUPING)
            ++ groupLevel;

          parent->GetParent(getter_AddRefs(nextParent));
          parent.swap(nextParent);
        }
      }

      nsAccUtils::SetAccGroupAttrs(attributes, groupLevel, positionInGroup,
                                   setSize);
    }
  }

  attributes.swap(*aAttributes);

  return NS_OK;
}

nsresult
nsAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(element, NS_ERROR_UNEXPECTED);

  nsAutoString tagName;
  element->GetTagName(tagName);
  if (!tagName.IsEmpty()) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("tag"), tagName,
                                   oldValueUnused);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GroupPosition(PRInt32 *aGroupLevel,
                            PRInt32 *aSimilarItemsInGroup,
                            PRInt32 *aPositionInGroup)
{
  // Every element exposes level/posinset/sizeset for IAccessdible::attributes
  // if they make sense for it. These attributes are mapped into groupPosition.
  // If 'level' attribute doesn't make sense element then it isn't represented
  // via IAccessible::attributes and groupLevel of groupPosition method is 0.
  // Elements that expose 'level' attribute only (like html headings elements)
  // don't support this method and all arguements are equealed 0.

  NS_ENSURE_ARG_POINTER(aGroupLevel);
  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  NS_ENSURE_ARG_POINTER(aPositionInGroup);

  *aGroupLevel = 0;
  *aSimilarItemsInGroup = 0;
  *aPositionInGroup = 0;

  nsCOMPtr<nsIPersistentProperties> attributes;
  nsresult rv = GetAttributes(getter_AddRefs(attributes));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!attributes) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 level, posInSet, setSize;
  nsAccUtils::GetAccGroupAttrs(attributes, &level, &posInSet, &setSize);

  if (!posInSet && !setSize)
    return NS_OK;

  *aGroupLevel = level;

  *aPositionInGroup = posInSet;
  *aSimilarItemsInGroup = setSize - 1;

  return NS_OK;
}

PRBool nsAccessible::MappedAttrState(nsIContent *aContent, PRUint32 *aStateInOut,
                                     nsStateMapEntry *aStateMapEntry)
{
  // Return true if we should continue
  if (!aStateMapEntry->attributeName) {
    return PR_FALSE;  // Stop looking -- no more states
  }

  nsAutoString attribValue;
  nsCOMPtr<nsIAtom> attribAtom = do_GetAtom(aStateMapEntry->attributeName); // XXX put atoms directly in entry
  NS_ENSURE_TRUE(attribAtom, NS_ERROR_OUT_OF_MEMORY);
  if (aContent->GetAttr(kNameSpaceID_WAIProperties, attribAtom, attribValue)) {
    if (aStateMapEntry->attributeValue == kBoolState) {
      // No attribute value map specified in state map entry indicates state cleared
      if (attribValue.EqualsLiteral("false")) {
        return *aStateInOut &= ~aStateMapEntry->state;
      }
      return *aStateInOut |= aStateMapEntry->state;
    }
    if (NS_ConvertUTF16toUTF8(attribValue).Equals(aStateMapEntry->attributeValue)) {
      return *aStateInOut |= aStateMapEntry->state;
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsAccessible::GetFinalState(PRUint32 *aState, PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsresult rv = GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply ARIA states to be sure accessible states will be overriden.
  *aState |= GetARIAState();

  // Set additional states which presence depends on another states.
  if (aExtraState) {
    if (!(*aState & nsIAccessibleStates::STATE_UNAVAILABLE)) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_ENABLED |
                      nsIAccessibleStates::EXT_STATE_SENSITIVE;
    }

    const PRUint32 kExpandCollapseStates =
      nsIAccessibleStates::STATE_COLLAPSED | nsIAccessibleStates::STATE_EXPANDED;
    if (*aState & kExpandCollapseStates) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_EXPANDABLE;
      if ((*aState & kExpandCollapseStates) == kExpandCollapseStates) {
        // Cannot be both expanded and collapsed -- this happens 
        // in ARIA expanded combobox because of limitation of nsARIAMap
        // XXX Perhaps we will be able to make this less hacky if 
        // we support extended states in nsARIAMap, e.g. derive
        // COLLAPSED from EXPANDABLE && !EXPANDED
        *aExtraState &= ~nsIAccessibleStates::STATE_COLLAPSED;
      } 
    }
    nsIFrame *frame = GetFrame();
    if (frame) {
      const nsStyleDisplay* display = frame->GetStyleDisplay();
      if (display && display->mOpacity == 1.0f &&
          !(*aState & nsIAccessibleStates::STATE_INVISIBLE)) {
        *aExtraState |= nsIAccessibleStates::EXT_STATE_OPAQUE;
      }

      const nsStyleXUL *xulStyle = frame->GetStyleXUL();
      if (xulStyle) {
        // In XUL all boxes are either vertical or horizontal
        if (xulStyle->mBoxOrient == NS_STYLE_BOX_ORIENT_VERTICAL) {
          *aExtraState |= nsIAccessibleStates::EXT_STATE_VERTICAL;
        }
        else {
          *aExtraState |= nsIAccessibleStates::EXT_STATE_HORIZONTAL;
        }
      }
    }

    // XXX We can remove this hack once we support RDF-based role & state maps
    if (mRoleMapEntry && (mRoleMapEntry->role == nsIAccessibleRole::ROLE_ENTRY ||
        mRoleMapEntry->role == nsIAccessibleRole::ROLE_PASSWORD_TEXT)) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
      NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
      if (content->AttrValueIs(kNameSpaceID_WAIProperties, nsAccessibilityAtoms::multiline,
                               nsAccessibilityAtoms::_true, eCaseMatters)) {
        *aExtraState |= nsIAccessibleStates::EXT_STATE_MULTI_LINE;
      }
      else {
        *aExtraState |= nsIAccessibleStates::EXT_STATE_SINGLE_LINE;
      }
    }
  }

  return NS_OK;
}

PRUint32
nsAccessible::GetARIAState()
{
  // Test for universal states first
  nsIContent *content = GetRoleContent(mDOMNode);
  if (!content) {
    return 0;
  }

  PRUint32 ariaState = 0;
  PRUint32 index = 0;
  while (nsARIAMap::gWAIUnivStateMap[index].attributeName != nsnull) {
    MappedAttrState(content, &ariaState, &nsARIAMap::gWAIUnivStateMap[index]);
    ++ index;
  }

  if (!mRoleMapEntry)
    return ariaState;

  // Once DHTML role is used, we're only readonly if DHTML readonly used
  ariaState &= ~nsIAccessibleStates::STATE_READONLY;

  if (ariaState & nsIAccessibleStates::STATE_UNAVAILABLE) {
    // Disabled elements are not selectable or focusable, even if disabled
    // via DHTML accessibility disabled property
    ariaState &= ~(nsIAccessibleStates::STATE_SELECTABLE |
                   nsIAccessibleStates::STATE_FOCUSABLE);
  }

  ariaState |= mRoleMapEntry->state;
  if (MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap1) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap2) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap3) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap4) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap5) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap6) &&
      MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap7)) {
    MappedAttrState(content, &ariaState, &mRoleMapEntry->attributeMap8);
  }

  return ariaState;
}

// Not implemented by this class

/* DOMString getValue (); */
NS_IMETHODIMP nsAccessible::GetValue(nsAString& aValue)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }
  if (mRoleMapEntry) {
    if (mRoleMapEntry->valueRule == eNoValue) {
      return NS_OK;
    }
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    if (content && content->GetAttr(kNameSpaceID_WAIProperties,
                                    nsAccessibilityAtoms::valuenow, aValue)) {
      return NS_OK;
    }
  }
  return NS_OK;
}

// nsIAccessibleValue
NS_IMETHODIMP
nsAccessible::GetMaximumValue(double *aMaximumValue)
{
  return GetAttrValue(kNameSpaceID_WAIProperties,
                      nsAccessibilityAtoms::valuemax, aMaximumValue);
}

NS_IMETHODIMP
nsAccessible::GetMinimumValue(double *aMinimumValue)
{
  return GetAttrValue(kNameSpaceID_WAIProperties,
                      nsAccessibilityAtoms::valuemin, aMinimumValue);
}

NS_IMETHODIMP
nsAccessible::GetMinimumIncrement(double *aMinIncrement)
{
  NS_ENSURE_ARG_POINTER(aMinIncrement);
  *aMinIncrement = 0;

  // No mimimum increment in dynamic content spec right now
  return NS_OK_NO_ARIA_VALUE;
}

NS_IMETHODIMP
nsAccessible::GetCurrentValue(double *aValue)
{
  return GetAttrValue(kNameSpaceID_WAIProperties,
                      nsAccessibilityAtoms::valuenow, aValue);
}

NS_IMETHODIMP
nsAccessible::SetCurrentValue(double aValue)
{
  if (!mDOMNode)
    return NS_ERROR_FAILURE;  // Node already shut down

  if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return NS_OK_NO_ARIA_VALUE;

  const PRUint32 kValueCannotChange = nsIAccessibleStates::STATE_READONLY |
                                      nsIAccessibleStates::STATE_UNAVAILABLE;

  if (State(this) & kValueCannotChange)
    return NS_ERROR_FAILURE;

  double minValue = 0;
  if (NS_SUCCEEDED(GetMinimumValue(&minValue)) && aValue < minValue)
    return NS_ERROR_INVALID_ARG;

  double maxValue = 0;
  if (NS_SUCCEEDED(GetMaximumValue(&maxValue)) && aValue > maxValue)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ENSURE_STATE(content);

  nsAutoString newValue;
  newValue.AppendFloat(aValue);
  return content->SetAttr(kNameSpaceID_WAIProperties,
                          nsAccessibilityAtoms::valuenow, newValue, PR_TRUE);
}

/* void setName (in DOMString name); */
NS_IMETHODIMP nsAccessible::SetName(const nsAString& name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAccessible::GetDefaultKeyBinding(nsAString& aKeyBinding)
{
  aKeyBinding.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetKeyBindings(PRUint8 aActionIndex,
                             nsIDOMDOMStringList **aKeyBindings)
{
  // Currently we support only unique key binding on element for default action.
  NS_ENSURE_TRUE(aActionIndex == 0, NS_ERROR_INVALID_ARG);

  nsAccessibleDOMStringList *keyBindings = new nsAccessibleDOMStringList();
  NS_ENSURE_TRUE(keyBindings, NS_ERROR_OUT_OF_MEMORY);

  nsAutoString defaultKey;
  nsresult rv = GetDefaultKeyBinding(defaultKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!defaultKey.IsEmpty())
    keyBindings->Add(defaultKey);

  NS_ADDREF(*aKeyBindings = keyBindings);
  return NS_OK;
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsAccessible::GetRole(PRUint32 *aRole)
{
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;
  return NS_OK;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 0;
  return NS_OK;
}

/* DOMString getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::GetActionName(PRUint8 index, nsAString& aName)
{
  return NS_ERROR_FAILURE;
}

/* DOMString getActionDescription (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::GetActionDescription(PRUint8 aIndex, nsAString& aDescription)
{
  // default to localized action name.
  nsAutoString name;
  nsresult rv = GetActionName(aIndex, name);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetTranslatedString(name, aDescription);
}

/* void doAction (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_FAILURE;
}

/* DOMString getHelp (); */
NS_IMETHODIMP nsAccessible::GetHelp(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToRight(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToRight(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToLeft(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToLeft(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleAbove(); */
NS_IMETHODIMP nsAccessible::GetAccessibleAbove(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleBelow(); */
NS_IMETHODIMP nsAccessible::GetAccessibleBelow(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<nsIDOMNode>
nsAccessible::FindNeighbourPointingToThis(nsIAtom *aRelationAttr,
                                          PRUint32 aRelationNameSpaceID,
                                          PRUint32 aAncestorLevelsToSearch)
{
  nsIContent *content = GetRoleContent(mDOMNode);
  if (!content)
    return nsnull; // Node shut down

  nsIContent* description = FindNeighbourPointingToNode(content, nsnull,
                                                        aRelationAttr,
                                                        aRelationNameSpaceID,
                                                        aAncestorLevelsToSearch);

  if (!description)
    return nsnull;

  nsIDOMNode *relatedNode;
  CallQueryInterface(description, &relatedNode);
  return relatedNode;
}


/* nsIAccessible getAccessibleRelated(); */
NS_IMETHODIMP nsAccessible::GetAccessibleRelated(PRUint32 aRelationType, nsIAccessible **aRelated)
{
  // When adding support for relations, make sure to add them to
  // appropriate places in nsAccessibleWrap implementations
  *aRelated = nsnull;

  // Relationships are defined on the same content node
  // that the role would be defined on
  nsIContent *content = GetRoleContent(mDOMNode);
  if (!content) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }

  nsCOMPtr<nsIDOMNode> relatedNode;
  nsAutoString relatedID;

  // Search for the related DOM node according to the specified "relation type"
  switch (aRelationType)
  {
  case nsIAccessibleRelation::RELATION_LABEL_FOR:
    {
      if (content->Tag() == nsAccessibilityAtoms::label) {
        nsIAtom *relatedIDAttr = content->IsNodeOfType(nsINode::eHTML) ?
          nsAccessibilityAtoms::_for : nsAccessibilityAtoms::control;
        content->GetAttr(kNameSpaceID_None, relatedIDAttr, relatedID);
      }
      if (relatedID.IsEmpty()) {
        const PRUint32 kAncestorLevelsToSearch = 3;
        relatedNode = FindNeighbourPointingToThis(nsAccessibilityAtoms::labelledby,
                                                  kNameSpaceID_WAIProperties,
                                                  kAncestorLevelsToSearch);
      }
      break;
    }
  case nsIAccessibleRelation::RELATION_LABELLED_BY:
    {
      content->GetAttr(kNameSpaceID_WAIProperties,
                       nsAccessibilityAtoms::labelledby, relatedID);
      if (relatedID.IsEmpty()) {
        relatedNode = do_QueryInterface(GetLabelContent(content));
      }
      break;
    }
  case nsIAccessibleRelation::RELATION_DESCRIBED_BY:
    {
      content->GetAttr(kNameSpaceID_WAIProperties,
                       nsAccessibilityAtoms::describedby, relatedID);
      if (relatedID.IsEmpty()) {
        nsIContent *description =
          FindNeighbourPointingToNode(content,
                                      nsAccessibilityAtoms::description,
                                      nsAccessibilityAtoms::control);

        relatedNode = do_QueryInterface(description);
      }
      break;
    }
  case nsIAccessibleRelation::RELATION_DESCRIPTION_FOR:
    {
      const PRUint32 kAncestorLevelsToSearch = 3;
      relatedNode =
        FindNeighbourPointingToThis(nsAccessibilityAtoms::describedby,
                                    kNameSpaceID_WAIProperties,
                                    kAncestorLevelsToSearch);

      if (!relatedNode && content->Tag() == nsAccessibilityAtoms::description &&
          content->IsNodeOfType(nsINode::eXUL)) {
        // This affectively adds an optional control attribute to xul:description,
        // which only affects accessibility, by allowing the description to be
        // tied to a control.
        content->GetAttr(kNameSpaceID_None,
                         nsAccessibilityAtoms::control, relatedID);
      }
      break;
    }
  case nsIAccessibleRelation::RELATION_NODE_CHILD_OF:
    {
      relatedNode = FindNeighbourPointingToThis(nsAccessibilityAtoms::owns,
                                                kNameSpaceID_WAIProperties);
      break;
    }
  case nsIAccessibleRelation::RELATION_CONTROLLED_BY:
    {
      relatedNode = FindNeighbourPointingToThis(nsAccessibilityAtoms::controls,
                                                kNameSpaceID_WAIProperties);
      break;
    }
  case nsIAccessibleRelation::RELATION_CONTROLLER_FOR:
    {
      content->GetAttr(kNameSpaceID_WAIProperties,
                       nsAccessibilityAtoms::controls, relatedID);
      break;
    }
  case nsIAccessibleRelation::RELATION_FLOWS_TO:
    {
      content->GetAttr(kNameSpaceID_WAIProperties,
                       nsAccessibilityAtoms::flowto, relatedID);
      break;
    }
  case nsIAccessibleRelation::RELATION_FLOWS_FROM:
    {
      relatedNode = FindNeighbourPointingToThis(nsAccessibilityAtoms::flowto,
                                                kNameSpaceID_WAIProperties);
      break;
    }

  case nsIAccessibleRelation::RELATION_DEFAULT_BUTTON:
    {
      if (content->IsNodeOfType(nsINode::eHTML)) {
        // HTML form controls implements nsIFormControl interface.
        nsCOMPtr<nsIFormControl> control(do_QueryInterface(content));
        if (control) {
          nsCOMPtr<nsIDOMHTMLFormElement> htmlform;
          control->GetForm(getter_AddRefs(htmlform));
          nsCOMPtr<nsIForm> form(do_QueryInterface(htmlform));
          if (form)
            relatedNode = do_QueryInterface(form->GetDefaultSubmitElement());
        }
      }
      else {
        // In XUL, use first <button default="true" .../> in the document
        nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(content->GetDocument());
        nsCOMPtr<nsIDOMXULButtonElement> buttonEl;
        if (xulDoc) {
          nsCOMPtr<nsIDOMNodeList> possibleDefaultButtons;
          xulDoc->GetElementsByAttribute(NS_LITERAL_STRING("default"),
                                         NS_LITERAL_STRING("true"),
                                         getter_AddRefs(possibleDefaultButtons));
          if (possibleDefaultButtons) {
            PRUint32 length;
            possibleDefaultButtons->GetLength(&length);
            nsCOMPtr<nsIDOMNode> possibleButton;
            // Check for button in list of default="true" elements
            for (PRUint32 count = 0; count < length && !buttonEl; count ++) {
              possibleDefaultButtons->Item(count, getter_AddRefs(possibleButton));
              buttonEl = do_QueryInterface(possibleButton);
            }
          }
          if (!buttonEl) { // Check for anonymous accept button in <dialog>
            nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(xulDoc));
            if (xblDoc) {
              nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(xulDoc);
              NS_ASSERTION(domDoc, "No DOM document");
              nsCOMPtr<nsIDOMElement> rootEl;
              domDoc->GetDocumentElement(getter_AddRefs(rootEl));
              if (rootEl) {
                nsCOMPtr<nsIDOMElement> possibleButtonEl;
                xblDoc->GetAnonymousElementByAttribute(rootEl,
                                                      NS_LITERAL_STRING("default"),
                                                      NS_LITERAL_STRING("true"),
                                                      getter_AddRefs(possibleButtonEl));
                buttonEl = do_QueryInterface(possibleButtonEl);
              }
            }
          }
          relatedNode = do_QueryInterface(buttonEl);
        }
      }
      break;
    }
  case nsIAccessibleRelation::RELATION_MEMBER_OF:
    {
      relatedNode = nsAccEvent::GetLastEventAtomicRegion(mDOMNode);
      break;
    }
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!relatedID.IsEmpty()) {
    // In some cases we need to get the relatedNode from an ID-style attribute
    nsCOMPtr<nsIDOMDocument> domDoc;
    mDOMNode->GetOwnerDocument(getter_AddRefs(domDoc));
    NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMElement> relatedEl;
    domDoc->GetElementById(relatedID, getter_AddRefs(relatedEl));
    relatedNode = do_QueryInterface(relatedEl);
  }

  // Return the corresponding accessible if the related DOM node is found
  if (relatedNode) {
    nsCOMPtr<nsIAccessibilityService> accService = GetAccService();
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
    accService->GetAccessibleInWeakShell(relatedNode, mWeakShell, aRelated);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAccessible::GetRelationsCount(PRUint32 *aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  nsCOMPtr<nsIArray> relations;
  nsresult rv = GetRelations(getter_AddRefs(relations));
  NS_ENSURE_SUCCESS(rv, rv);

  return relations->GetLength(aCount);
}

NS_IMETHODIMP
nsAccessible::GetRelation(PRUint32 aIndex, nsIAccessibleRelation **aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nsnull;

  nsCOMPtr<nsIArray> relations;
  nsresult rv = GetRelations(getter_AddRefs(relations));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessibleRelation> relation;
  rv = relations->QueryElementAt(aIndex, NS_GET_IID(nsIAccessibleRelation),
                                 getter_AddRefs(relation));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aRelation = relation);
  return rv;
}

NS_IMETHODIMP
nsAccessible::GetRelations(nsIArray **aRelations)
{
  NS_ENSURE_ARG_POINTER(aRelations);

  nsCOMPtr<nsIMutableArray> relations = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(relations, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 relType = nsIAccessibleRelation::RELATION_FIRST;
       relType < nsIAccessibleRelation::RELATION_LAST;
       ++relType) {
    nsCOMPtr<nsIAccessible> accessible;
    GetAccessibleRelated(relType, getter_AddRefs(accessible));

    if (accessible) {
      nsCOMPtr<nsIAccessibleRelation> relation =
        new nsAccessibleRelationWrap(relType, accessible);
      NS_ENSURE_TRUE(relation, NS_ERROR_OUT_OF_MEMORY);

      relations->AppendElement(relation, PR_FALSE);
    }
  }

  NS_ADDREF(*aRelations = relations);
  return NS_OK;
}

/* void extendSelection (); */
NS_IMETHODIMP nsAccessible::ExtendSelection()
{
  // XXX Should be implemented, but not high priority
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void getNativeInterface(out voidPtr aOutAccessible); */
NS_IMETHODIMP nsAccessible::GetNativeInterface(void **aOutAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsAccessible::DoCommandCallback(nsITimer *aTimer, void *aClosure)
{
  NS_ASSERTION(gDoCommandTimer, "How did we get here if there was no gDoCommandTimer?");
  NS_RELEASE(gDoCommandTimer);

  nsIContent *content = reinterpret_cast<nsIContent*>(aClosure);
  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(content));
  if (xulElement) {
    xulElement->Click();
  }
  else {
    nsIDocument *doc = content->GetDocument();
    if (!doc) {
      return;
    }
    nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
    nsPIDOMWindow *outerWindow = doc->GetWindow();
    if (presShell && outerWindow) {
      nsAutoPopupStatePusher popupStatePusher(outerWindow, openAllowed);

      nsMouseEvent downEvent(PR_TRUE, NS_MOUSE_BUTTON_DOWN, nsnull,
                             nsMouseEvent::eSynthesized);
      nsMouseEvent upEvent(PR_TRUE, NS_MOUSE_BUTTON_UP, nsnull,
                           nsMouseEvent::eSynthesized);
      nsMouseEvent clickEvent(PR_TRUE, NS_MOUSE_CLICK, nsnull,
                              nsMouseEvent::eSynthesized);

      nsEventStatus eventStatus = nsEventStatus_eIgnore;
      content->DispatchDOMEvent(&downEvent, nsnull,
                                 presShell->GetPresContext(), &eventStatus);
      content->DispatchDOMEvent(&upEvent, nsnull,
                                 presShell->GetPresContext(), &eventStatus);
      content->DispatchDOMEvent(&clickEvent, nsnull,
                                 presShell->GetPresContext(), &eventStatus);
    }
  }
}

/*
 * Use Timer to execute "Click" command of XUL/HTML element (e.g. menuitem, button...).
 *
 * When "Click" is to open a "modal" dialog/window, it won't return untill the
 * dialog/window is closed. If executing "Click" command directly in
 * nsXXXAccessible::DoAction, it will block AT-Tools(e.g. GOK) that invoke
 * "action" of mozilla accessibles direclty.
 */
nsresult nsAccessible::DoCommand(nsIContent *aContent)
{
  nsCOMPtr<nsIContent> content = aContent;
  if (!content) {
    content = do_QueryInterface(mDOMNode);
  }
  if (gDoCommandTimer) {
    // Already have timer going for another command
    NS_WARNING("Doubling up on do command timers doesn't work. This wasn't expected.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  if (!timer) {
    return NS_ERROR_OUT_OF_MEMORY;
  } 

  NS_ADDREF(gDoCommandTimer = timer);
  return gDoCommandTimer->InitWithFuncCallback(DoCommandCallback,
                                               (void*)content, 0,
                                               nsITimer::TYPE_ONE_SHOT);
}

already_AddRefed<nsIAccessible>
nsAccessible::GetNextWithState(nsIAccessible *aStart, PRUint32 matchState)
{
  // Return the next descendant that matches one of the states in matchState
  // Uses depth first search
  NS_ASSERTION(matchState, "GetNextWithState() not called with a state to match");
  NS_ASSERTION(aStart, "GetNextWithState() not called with an accessible to start with");
  nsCOMPtr<nsIAccessible> look, current = aStart;
  PRUint32 state = 0;
  while (0 == (state & matchState)) {
    current->GetFirstChild(getter_AddRefs(look));
    while (!look) {
      if (current == this) {
        return nsnull; // At top of subtree
      }
      current->GetNextSibling(getter_AddRefs(look));
      if (!look) {
        current->GetParent(getter_AddRefs(look));
        current.swap(look);
        continue;
      }
    }
    current.swap(look);
    state = State(current);
  }

  nsIAccessible *returnAccessible = nsnull;
  current.swap(returnAccessible);

  return returnAccessible;
}

// nsIAccessibleSelectable
NS_IMETHODIMP nsAccessible::GetSelectedChildren(nsIArray **aSelectedAccessibles)
{
  *aSelectedAccessibles = nsnull;

  nsCOMPtr<nsIMutableArray> selectedAccessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(selectedAccessibles);

  nsCOMPtr<nsIAccessible> selected = this;
  while ((selected = GetNextWithState(selected, nsIAccessibleStates::STATE_SELECTED)) != nsnull) {
    selectedAccessibles->AppendElement(selected, PR_FALSE);
  }

  PRUint32 length = 0;
  selectedAccessibles->GetLength(&length); 
  if (length) { // length of nsIArray containing selected options
    *aSelectedAccessibles = selectedAccessibles;
    NS_ADDREF(*aSelectedAccessibles);
  }

  return NS_OK;
}

// return the nth selected descendant nsIAccessible object
NS_IMETHODIMP nsAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **aSelected)
{
  *aSelected = nsnull;
  if (aIndex < 0) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIAccessible> selected = this;
  PRInt32 count = 0;
  while (count ++ <= aIndex) {
    selected = GetNextWithState(selected, nsIAccessibleStates::STATE_SELECTED);
    if (!selected) {
      return NS_ERROR_FAILURE; // aIndex out of range
    }
  }
  NS_IF_ADDREF(*aSelected = selected);
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;
  nsCOMPtr<nsIAccessible> selected = this;
  while ((selected = GetNextWithState(selected, nsIAccessibleStates::STATE_SELECTED)) != nsnull) {
    ++ *aSelectionCount;
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::AddChildToSelection(PRInt32 aIndex)
{
  // Tree views and other container widgets which may have grandchildren should
  // implement a selection methods for their specific interfaces, because being
  // able to deal with selection on a per-child basis would not be enough.

  NS_ENSURE_TRUE(aIndex >= 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessible> child;
  GetChildAt(aIndex, getter_AddRefs(child));

  PRUint32 state = State(child);
  if (!(state & nsIAccessibleStates::STATE_SELECTABLE)) {
    return NS_OK;
  }

  return child->SetSelected(PR_TRUE);
}

NS_IMETHODIMP nsAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  // Tree views and other container widgets which may have grandchildren should
  // implement a selection methods for their specific interfaces, because being
  // able to deal with selection on a per-child basis would not be enough.

  NS_ENSURE_TRUE(aIndex >= 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessible> child;
  GetChildAt(aIndex, getter_AddRefs(child));

  PRUint32 state = State(child);
  if (!(state & nsIAccessibleStates::STATE_SELECTED)) {
    return NS_OK;
  }

  return child->SetSelected(PR_FALSE);
}

NS_IMETHODIMP nsAccessible::IsChildSelected(PRInt32 aIndex, PRBool *aIsSelected)
{
  // Tree views and other container widgets which may have grandchildren should
  // implement a selection methods for their specific interfaces, because being
  // able to deal with selection on a per-child basis would not be enough.

  *aIsSelected = PR_FALSE;
  NS_ENSURE_TRUE(aIndex >= 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAccessible> child;
  GetChildAt(aIndex, getter_AddRefs(child));

  PRUint32 state = State(child);
  if (state & nsIAccessibleStates::STATE_SELECTED) {
    *aIsSelected = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::ClearSelection()
{
  nsCOMPtr<nsIAccessible> selected = this;
  while ((selected = GetNextWithState(selected, nsIAccessibleStates::STATE_SELECTED)) != nsnull) {
    selected->SetSelected(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SelectAllSelection(PRBool *_retval)
{
  nsCOMPtr<nsIAccessible> selectable = this;
  while ((selectable = GetNextWithState(selectable, nsIAccessibleStates::STATE_SELECTED)) != nsnull) {
    selectable->SetSelected(PR_TRUE);
  }
  return NS_OK;
}

// nsIAccessibleHyperLink
// Because of new-atk design, any embedded object in text can implement
// nsIAccessibleHyperLink, which helps determine where it is located
// within containing text

NS_IMETHODIMP nsAccessible::GetAnchors(PRInt32 *aAnchors)
{
  *aAnchors = 1;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetStartIndex(PRInt32 *aStartIndex)
{
  *aStartIndex = 0;
  PRInt32 endIndex;
  return GetLinkOffset(aStartIndex, &endIndex);
}

NS_IMETHODIMP nsAccessible::GetEndIndex(PRInt32 *aEndIndex)
{
  *aEndIndex = 0;
  PRInt32 startIndex;
  return GetLinkOffset(&startIndex, aEndIndex);
}

NS_IMETHODIMP nsAccessible::GetURI(PRInt32 i, nsIURI **aURI)
{
  *aURI = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessible::GetObject(PRInt32 aIndex,
                                      nsIAccessible **aAccessible)
{
  if (aIndex != 0) {
    *aAccessible = nsnull;
    return NS_ERROR_FAILURE;
  }
  *aAccessible = this;
  NS_ADDREF_THIS();
  return NS_OK;
}

// nsIAccessibleHyperLink::IsValid()
NS_IMETHODIMP nsAccessible::IsValid(PRBool *aIsValid)
{
  PRUint32 state = State(this);
  *aIsValid = (state & nsIAccessibleStates::STATE_INVALID) != 0;
  // XXX In order to implement this we would need to follow every link
  // Perhaps we can get information about invalid links from the cache
  // In the mean time authors can use role="wairole:link" aaa:invalid="true"
  // to force it for links they internally know to be invalid
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::IsSelected(PRBool *aIsSelected)
{
  *aIsSelected = (gLastFocusedNode == mDOMNode);
  return NS_OK;
}

nsresult nsAccessible::GetLinkOffset(PRInt32* aStartOffset, PRInt32* aEndOffset)
{
  *aStartOffset = *aEndOffset = 0;
  nsCOMPtr<nsIAccessible> parent(GetParent());
  if (!parent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible, nextSibling;
  PRInt32 characterCount = 0;
  parent->GetFirstChild(getter_AddRefs(accessible));

  while (accessible) {
    if (IsText(accessible)) {
      characterCount += TextLength(accessible);
    }
    else if (accessible == this) {
      *aStartOffset = characterCount;
      *aEndOffset = characterCount + 1;
      return NS_OK;
    }
    else {
      ++ characterCount;
    }
    accessible->GetNextSibling(getter_AddRefs(nextSibling));
    accessible.swap(nextSibling);
  }

  return NS_ERROR_FAILURE;
}

PRInt32 nsAccessible::TextLength(nsIAccessible *aAccessible)
{
  if (!IsText(aAccessible))
    return 1;

  nsCOMPtr<nsPIAccessNode> pAccNode(do_QueryInterface(aAccessible));
  NS_ASSERTION(pAccNode, "QI to nsPIAccessNode failed");

  nsIFrame *frame = pAccNode->GetFrame();
  if (frame && frame->GetType() == nsAccessibilityAtoms::textFrame) {
    // Ensure that correct text length is calculated (with non-rendered whitespace chars not counted)
    nsIContent *content = frame->GetContent();
    if (content) {
      PRUint32 length;
      nsresult rv = nsHyperTextAccessible::ContentToRenderedOffset(frame, content->TextLength(), &length);
      return NS_SUCCEEDED(rv) ? static_cast<PRInt32>(length) : -1;
    }
  }

  // For list bullets (or anything other accessible which would compute its own text
  // They don't have their own frame.
  // XXX In the future, list bullets may have frame and anon content, so 
  // we should be able to remove this at that point
  nsCOMPtr<nsPIAccessible> pAcc(do_QueryInterface(aAccessible));
  NS_ASSERTION(pAcc, "QI to nsPIAccessible failed");

  nsAutoString text;
  pAcc->AppendTextTo(text, 0, PR_UINT32_MAX); // Get all the text
  return text.Length();
}

NS_IMETHODIMP
nsAccessible::AppendTextTo(nsAString& aText, PRUint32 aStartOffset, PRUint32 aLength)
{
  return NS_OK;
}

already_AddRefed<nsIAccessible>
nsAccessible::GetFirstAvailableAccessible(nsIDOMNode *aStartNode, PRBool aRequireLeaf)
{
  nsIAccessibilityService *accService = GetAccService();
  nsCOMPtr<nsIAccessible> accessible;
  nsCOMPtr<nsIDOMTreeWalker> walker; 
  nsCOMPtr<nsIDOMNode> currentNode(aStartNode);

  while (currentNode) {
    accService->GetAccessibleInWeakShell(currentNode, mWeakShell, getter_AddRefs(accessible)); // AddRef'd
    if (accessible && (!aRequireLeaf || IsLeaf(accessible))) {
      nsIAccessible *retAccessible = accessible;
      NS_ADDREF(retAccessible);
      return retAccessible;
    }
    if (!walker) {
      // Instantiate walker lazily since we won't need it in 90% of the cases
      // where the first DOM node we're given provides an accessible
      nsCOMPtr<nsIDOMDocument> document;
      currentNode->GetOwnerDocument(getter_AddRefs(document));
      nsCOMPtr<nsIDOMDocumentTraversal> trav = do_QueryInterface(document);
      NS_ASSERTION(trav, "No DOM document traversal for document");
      NS_ENSURE_TRUE(trav, nsnull);
      trav->CreateTreeWalker(mDOMNode, nsIDOMNodeFilter::SHOW_ELEMENT | nsIDOMNodeFilter::SHOW_TEXT,
                            nsnull, PR_FALSE, getter_AddRefs(walker));
      NS_ENSURE_TRUE(walker, nsnull);
      walker->SetCurrentNode(currentNode);
    }

    walker->NextNode(getter_AddRefs(currentNode));
  }

  return nsnull;
}

PRBool nsAccessible::CheckVisibilityInParentChain(nsIDocument* aDocument, nsIView* aView)
{
  nsIDocument* document = aDocument;
  nsIView* view = aView;
  // both view chain and widget chain are broken between chrome and content
  while (document != nsnull) {
    while (view != nsnull) {
      if (view->GetVisibility() == nsViewVisibility_kHide) {
        return PR_FALSE;
      }
      view = view->GetParent();
    }

    nsIDocument* parentDoc = document->GetParentDocument();
    if (parentDoc != nsnull) {
      nsIContent* content = parentDoc->FindContentForSubDocument(document);
      if (content != nsnull) {
        nsIPresShell* shell = parentDoc->GetPrimaryShell();
        nsIFrame* frame = shell->GetPrimaryFrameFor(content);
        while (frame != nsnull && !frame->HasView()) {
          frame = frame->GetParent();
        }

        if (frame != nsnull) {
          view = frame->GetViewExternal();
        }
      }
    }

    document = parentDoc;
  }

  return PR_TRUE;
}

nsresult
nsAccessible::GetAttrValue(PRUint32 aNameSpaceID, nsIAtom *aName,
                           double *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;

  if (!mDOMNode)
    return NS_ERROR_FAILURE;  // Node already shut down

 if (!mRoleMapEntry || mRoleMapEntry->valueRule == eNoValue)
    return NS_OK_NO_ARIA_VALUE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ENSURE_STATE(content);

  PRInt32 result = NS_OK;
  nsAutoString value;
  if (content->GetAttr(aNameSpaceID, aName, value) && !value.IsEmpty())
    *aValue = value.ToFloat(&result);

  return result;
}

PRBool nsAccessible::MustPrune(nsIAccessible *aAccessible)
{ 
  PRUint32 role = Role(aAccessible);
  return role == nsIAccessibleRole::ROLE_MENUITEM || 
         role == nsIAccessibleRole::ROLE_ENTRY ||
         role == nsIAccessibleRole::ROLE_PASSWORD_TEXT ||
         role == nsIAccessibleRole::ROLE_PUSHBUTTON ||
         role == nsIAccessibleRole::ROLE_TOGGLE_BUTTON ||
         role == nsIAccessibleRole::ROLE_GRAPHIC ||
         role == nsIAccessibleRole::ROLE_SLIDER ||
         role == nsIAccessibleRole::ROLE_PROGRESSBAR ||
         role == nsIAccessibleRole::ROLE_SEPARATOR;
}
