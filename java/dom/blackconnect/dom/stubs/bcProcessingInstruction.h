#ifndef __bcProcessingInstruction_h__
#define __bcProcessingInstruction_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMProcessingInstruction.h"

#define DOM_PROCESSINGINSTRUCTION_PTR(_processinginstruction_) (!_processinginstruction_ ? nsnull : ((bcProcessingInstruction*)_processinginstruction_)->domPtr)
#define NEW_BCPROCESSINGINSTRUCTION(_processinginstruction_) (!_processinginstruction_ ? nsnull : new bcProcessingInstruction(_processinginstruction_))

class bcProcessingInstruction : public ProcessingInstruction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PROCESSINGINSTRUCTION
  NS_FORWARD_NODE(nodePtr->)

  bcProcessingInstruction(nsIDOMProcessingInstruction* domPtr);
  virtual ~bcProcessingInstruction();
private:
  nsIDOMProcessingInstruction* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcProcessingInstruction_h__
