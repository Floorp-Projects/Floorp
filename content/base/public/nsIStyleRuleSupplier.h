#ifndef _nsIStyleRuleSupplier_h
#define _nsIStyleRuleSupplier_h

#include "nsISupports.h"
#include "nsISupportsArray.h"
#include "nsIStyleRuleProcessor.h"

// {2D77A45B-4F3A-4203-A7D2-F4B84D0C1EE4}
#define NS_ISTYLERULESUPPLIER_IID \
{ 0x2d77a45b, 0x4f3a, 0x4203, { 0xa7, 0xd2, 0xf4, 0xb8, 0x4d, 0xc, 0x1e, 0xe4 } }

class nsIContent;
class nsIStyleSet;

class nsIStyleRuleSupplier : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLERULESUPPLIER_IID)

  NS_IMETHOD UseDocumentRules(nsIContent* aContent, PRBool* aResult)=0;
  NS_IMETHOD WalkRules(nsIStyleSet* aStyleSet, 
                       nsISupportsArrayEnumFunc aFunc,
                       RuleProcessorData* aData)=0;

  NS_IMETHOD AttributeAffectsStyle(nsISupportsArrayEnumFunc aFunc,
                                   void* aData,
                                   nsIContent* aContent,
                                   PRBool* aAffects)=0;
};

#endif /* _nsIStyleRuleSupplier_h */
