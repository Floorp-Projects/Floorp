#ifndef __bcNotation_h__
#define __bcNotation_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMNotation.h"

#define DOM_NOTATION_PTR(_notation_) (!_notation_ ? nsnull : ((bcNotation*)_notation_)->domPtr)
#define NEW_BCNOTATION(_notation_) (!_notation_ ? nsnull : new bcNotation(_notation_))

class bcNotation : public Notation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NOTATION
  NS_FORWARD_NODE(nodePtr->)

  bcNotation(nsIDOMNotation* domPtr);
  virtual ~bcNotation();
private:
  nsIDOMNotation* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcNotation_h__
