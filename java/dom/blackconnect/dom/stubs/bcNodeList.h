#ifndef __bcNodeList_h__
#define __bcNodeList_h__

#include "dom.h"
#include "nsIDOMNodeList.h"

#define DOM_NODELIST_PTR(_nodelist_) (!_nodelist_ ? nsnull : ((bcNodeList*)_nodelist_)->domPtr)
#define NEW_BCNODELIST(_nodelist_) (!_nodelist_ ? nsnull : new bcNodeList(_nodelist_))

class bcNodeList : public NodeList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NODELIST

  bcNodeList(nsIDOMNodeList* domPtr);
  virtual ~bcNodeList();
private:
  nsIDOMNodeList* domPtr;
  /* additional members */
};

#endif //  __bcNodeList_h__
