#include "bcProcessingInstruction.h"

NS_IMPL_ISUPPORTS1(bcProcessingInstruction, ProcessingInstruction)

bcProcessingInstruction::bcProcessingInstruction(nsIDOMProcessingInstruction* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcProcessingInstruction::~bcProcessingInstruction()
{
  /* destructor code */
}

/* readonly attribute DOMString target; */
NS_IMETHODIMP bcProcessingInstruction::GetTarget(DOMString *aTarget)
{
  nsString ret;
  nsresult rv = domPtr->GetTarget(ret);
  *aTarget = ret.ToNewUnicode();
  return rv;
}


/* attribute DOMString data; */
NS_IMETHODIMP bcProcessingInstruction::GetData(DOMString *aData)
{
  nsString ret;
  nsresult rv = domPtr->GetData(ret);
  *aData = ret.ToNewUnicode();
  return rv;
}

NS_IMETHODIMP bcProcessingInstruction::SetData(DOMString aData)
{
  nsString _aData(aData);
  return domPtr->SetData(_aData);
}


