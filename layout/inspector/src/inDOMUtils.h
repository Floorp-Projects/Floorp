/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inDOMUtils_h__
#define __inDOMUtils_h__

#include "inIDOMUtils.h"

#include "nsISupportsArray.h"

class nsRuleNode;
class nsStyleContext;
class nsIAtom;
class nsIContent;

class inDOMUtils : public inIDOMUtils
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIDOMUTILS

  inDOMUtils();
  virtual ~inDOMUtils();

private:
  // aStyleContext must be released by the caller once he's done with aRuleNode.
  static nsresult GetRuleNodeForContent(nsIContent* aContent,
                                        nsIAtom* aPseudo,
                                        nsStyleContext** aStyleContext,
                                        nsRuleNode** aRuleNode);
};

// {0a499822-a287-4089-ad3f-9ffcd4f40263}
#define IN_DOMUTILS_CID \
  {0x0a499822, 0xa287, 0x4089, {0xad, 0x3f, 0x9f, 0xfc, 0xd4, 0xf4, 0x02, 0x63}}

#endif // __inDOMUtils_h__
