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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "nsXMLElement.h"
#include "nsHTMLAtoms.h"

class nsXMLEventsElement : public nsXMLElement {
public:
  nsXMLEventsElement(nsINodeInfo *aNodeInfo);
  virtual ~nsXMLEventsElement();
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsXMLElement::)
  virtual nsIAtom *GetIDAttributeName() const;
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
};

nsXMLEventsElement::nsXMLEventsElement(nsINodeInfo *aNodeInfo)
  : nsXMLElement(aNodeInfo)
{
}

nsXMLEventsElement::~nsXMLEventsElement()
{
}

nsIAtom *
nsXMLEventsElement::GetIDAttributeName() const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::listener))
    return nsHTMLAtoms::id;
  return nsXMLElement::GetIDAttributeName();
}

nsresult
nsXMLEventsElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                            const nsAString& aValue, PRBool aNotify)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::listener) && 
      mNodeInfo->GetDocument() && aNameSpaceID == kNameSpaceID_None && 
      aName == nsHTMLAtoms::_event)
    mNodeInfo->GetDocument()->AddXMLEventsContent(this);
  return nsXMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                   aNotify);
}

NS_IMETHODIMP
nsXMLEventsElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  nsXMLEventsElement* it = new nsXMLEventsElement(mNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  CopyInnerTo(it, aDeep);
  kungFuDeathGrip.swap(*aReturn);

  return NS_OK;
}

nsresult
NS_NewXMLEventsElement(nsIContent** aInstancePtrResult, nsINodeInfo *aNodeInfo)
{
  nsXMLEventsElement* it = new nsXMLEventsElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aInstancePtrResult = it);
  return NS_OK;
}

