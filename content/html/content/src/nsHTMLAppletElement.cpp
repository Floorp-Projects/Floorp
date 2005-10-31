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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDocument.h"

#include "nsObjectLoadingContent.h"

// XXX this is to get around conflicts with windows.h defines
// introduced through jni.h
#if defined (XP_WIN) && ! defined (WINCE)
#undef GetClassName
#undef GetObject
#endif


class nsHTMLAppletElement : public nsGenericHTMLElement,
                            public nsObjectLoadingContent,
                            public nsIDOMHTMLAppletElement
{
public:
  nsHTMLAppletElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLAppletElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLAppletElement
  NS_DECL_NSIDOMHTMLAPPLETELEMENT

  virtual void DoneAddingChildren(PRBool aHaveNotified);
  virtual PRBool IsDoneAddingChildren();

  // nsIContent
  // Have to override tabindex for <embed> to act right
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  // Let applet decide whether it wants focus from mouse clicks
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull)
    { if (aTabIndex) GetTabIndex(aTabIndex); return PR_TRUE; }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual PRInt32 IntrinsicState() const;

  // nsObjectLoadingContent
  virtual PRUint32 GetCapabilities() const;
protected:
  /**
   * Calls ObjectURIChanged with the correct arguments to start the plugin
   * load.
   */
  NS_HIDDEN_(void) StartAppletLoad(PRBool aNotify);

  PRPackedBool mReflectedApplet;
  PRPackedBool mIsDoneAddingChildren;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Applet)


nsHTMLAppletElement::nsHTMLAppletElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
  : nsGenericHTMLElement(aNodeInfo), mReflectedApplet(PR_FALSE),
    mIsDoneAddingChildren(!aFromParser)
{
}

nsHTMLAppletElement::~nsHTMLAppletElement()
{
}

PRBool
nsHTMLAppletElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}


PRUint32
nsHTMLAppletElement::GetCapabilities() const
{
  return eSupportPlugins;
}

void
nsHTMLAppletElement::DoneAddingChildren(PRBool aHaveNotified)
{
  mIsDoneAddingChildren = PR_TRUE;
  // If we're already in the document, start the load, because BindToTree
  // didn't.
  if (IsInDoc()) {
    StartAppletLoad(aHaveNotified);
  }
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAppletElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAppletElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLAppletElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLAppletElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLAppletElement)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoaderOwner)
  NS_INTERFACE_MAP_ENTRY(nsIObjectLoadingContent)
  NS_INTERFACE_MAP_ENTRY(nsIImageLoadingContent)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLAppletElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_DOM_CLONENODE(nsHTMLAppletElement)


NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Code, code)
NS_IMPL_URI_ATTR(nsHTMLAppletElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Height, height)
NS_IMPL_INT_ATTR(nsHTMLAppletElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Object, object)
NS_IMPL_INT_ATTR(nsHTMLAppletElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Width, width)
NS_IMPL_INT_ATTR(nsHTMLAppletElement, TabIndex, tabindex)
  
PRBool
nsHTMLAppletElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    return nsGenericHTMLElement::ParseAlignValue(aValue, aResult);
  }
  if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                aValue, aResult)) {
    return PR_TRUE;
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLAppletElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageAlignAttributeMap,
    sImageBorderAttributeMap
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

nsMapRuleToAttributesFunc
nsHTMLAppletElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult
nsHTMLAppletElement::BindToTree(nsIDocument* aDocument,
                                nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Must start loading stuff after being in the document, but only when we
  // already have all our children.
  if (mIsDoneAddingChildren) {
    // Don't need to notify: We have no frames yet, since we weren't in a
    // document
    StartAppletLoad(PR_FALSE);
  }
  return rv;
}


void
nsHTMLAppletElement::UnbindFromTree(PRBool aDeep,
                                    PRBool aNullParent)
{
  RemovedFromDocument();
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsHTMLAppletElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  // If we plan to call ObjectURIChanged, we want to do it first so that the
  // object load kicks off _before_ the reflow triggered by the SetAttr.  But if
  // aNotify is false, we are coming from the parser or some such place; we'll
  // get bound after all the attributes have been set, so we'll do the
  // object load from BindToTree/DoneAddingChildren.
  // Skip the ObjectURIChanged call in that case.
  if (aNotify &&
      aNameSpaceID == kNameSpaceID_None && aName == nsHTMLAtoms::code) {
    ObjectURIChanged(aValue, aNotify,
                     NS_LITERAL_CSTRING("application/x-java-vm"),
                     PR_TRUE, PR_TRUE);
  }


  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                       aValue, aNotify);
}

void
nsHTMLAppletElement::StartAppletLoad(PRBool aNotify)
{
  nsAutoString uri;
  if (GetAttr(kNameSpaceID_None, nsHTMLAtoms::code, uri)) {
    ObjectURIChanged(uri, aNotify,
                     NS_LITERAL_CSTRING("application/x-java-vm"), PR_TRUE);
  } else {
    // The constructor set the type to eType_Plugin; but if we have no code
    // attribute, then we aren't really a plugin
    Fallback(aNotify);
  }
}

PRInt32
nsHTMLAppletElement::IntrinsicState() const
{
  return nsGenericHTMLElement::IntrinsicState() | ObjectState();
}
