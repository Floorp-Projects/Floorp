#include "nsGUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIAtom.h"
#include "nsIDOMEventTarget.h"

struct nsMutationEvent : public nsEvent
{
  nsCOMPtr<nsIDOMNode> mRelatedNode;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  
  nsCOMPtr<nsIAtom> mAttrName;
  nsCOMPtr<nsIAtom> mPrevAttrValue;
  nsCOMPtr<nsIAtom> mNewAttrValue;
  
  unsigned short mAttrChange;
};

#define NS_MUTATION_EVENT     20

#define NS_MUTATION_START           1800
#define NS_MUTATION_SUBTREEMODIFIED                   (NS_MUTATION_START)
#define NS_MUTATION_NODEINSERTED                      (NS_MUTATION_START+1)
#define NS_MUTATION_NODEREMOVED                       (NS_MUTATION_START+2)
#define NS_MUTATION_NODEREMOVEDFROMDOCUMENT           (NS_MUTATION_START+3)
#define NS_MUTATION_NODEINSERTEDINTODOCUMENT          (NS_MUTATION_START+4)
#define NS_MUTATION_ATTRMODIFIED                      (NS_MUTATION_START+5)
#define NS_MUTATION_CHARACTERDATAMODIFIED             (NS_MUTATION_START+6)

// Bits are actually checked to optimize mutation event firing.
// That's why I don't number from 0x00.  The first event should
// always be 0x01.
#define NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED                0x01
#define NS_EVENT_BITS_MUTATION_NODEINSERTED                   0x02
#define NS_EVENT_BITS_MUTATION_NODEREMOVED                    0x04
#define NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT        0x08
#define NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT       0x10
#define NS_EVENT_BITS_MUTATION_ATTRMODIFIED                   0x20
#define NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED          0x40
