#include "bcCharacterData.h"

NS_IMPL_ISUPPORTS1(bcCharacterData, CharacterData)

bcCharacterData::bcCharacterData(nsIDOMCharacterData* ptr)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  domPtr = ptr;
  nodePtr = new bcNode(ptr);
}

bcCharacterData::~bcCharacterData()
{
  /* destructor code */
}

/* attribute DOMString data; */
NS_IMETHODIMP bcCharacterData::GetData(DOMString *aData)
{
  nsString ret;
  nsresult rv = domPtr->GetData(ret);
  *aData = ret.ToNewUnicode();
  return rv;
}

NS_IMETHODIMP bcCharacterData::SetData(DOMString aData)
{
  nsString _aData(aData);
  return domPtr->SetData(_aData);
}


/* readonly attribute unsigned long length; */
NS_IMETHODIMP bcCharacterData::GetLength(PRUint32 *aLength)
{
  return domPtr->GetLength(aLength);
}


/* DOMString substringData (in unsigned long offset, in unsigned long count)  raises (DOMException); */
NS_IMETHODIMP bcCharacterData::SubstringData(PRUint32 offset, PRUint32 count, DOMString *_retval)
{
  nsString ret;
  nsresult rv = domPtr->SubstringData(offset, count, ret);
  *_retval = ret.ToNewUnicode();
  return rv;
}

/* void appendData (in DOMString arg)  raises (DOMException); */
NS_IMETHODIMP bcCharacterData::AppendData(DOMString arg)
{
  nsString _arg(arg);
  return domPtr->AppendData(_arg);
}

/* void insertData (in unsigned long offset, in DOMString arg)  raises (DOMException); */
NS_IMETHODIMP bcCharacterData::InsertData(PRUint32 offset, DOMString arg)
{
  nsString _arg(arg);
  return domPtr->InsertData(offset, _arg);
}

/* void deleteData (in unsigned long offset, in unsigned long count)  raises (DOMException); */
NS_IMETHODIMP bcCharacterData::DeleteData(PRUint32 offset, PRUint32 count)
{
  return domPtr->DeleteData(offset, count);
}

/* void replaceData (in unsigned long offset, in unsigned long count, in DOMString arg)  raises (DOMException); */
NS_IMETHODIMP bcCharacterData::ReplaceData(PRUint32 offset, PRUint32 count, DOMString arg)
{
  nsString _arg(arg);
  return domPtr->ReplaceData(offset, count, _arg);
}
