#ifndef __bcNamedNodeMap_h__
#define __bcNamedNodeMap_h__

#include "dom.h"
#include "nsIDOMNamedNodeMap.h"

#define DOM_NAMEDNODEMAP_PTR(_namednodemap_) (!_namednodemap_ ? nsnull : ((bcNamedNodeMap*)_namednodemap_)->domPtr)
#define NEW_BCNAMEDNODEMAP(_namednodemap_) (!_namednodemap_ ? nsnull : new bcNamedNodeMap(_namednodemap_))

class bcNamedNodeMap : public NamedNodeMap
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NAMEDNODEMAP

  bcNamedNodeMap(nsIDOMNamedNodeMap* domPtr);
  virtual ~bcNamedNodeMap();
private:
  nsIDOMNamedNodeMap* domPtr;
  /* additional members */
};

#endif //  __bcNamedNodeMap_h__
