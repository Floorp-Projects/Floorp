#ifndef __bcEntity_h__
#define __bcEntity_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMEntity.h"

#define DOM_ENTITY_PTR(_entity_) (!_entity_ ? nsnull : ((bcEntity*)_entity_)->domPtr)
#define NEW_BCENTITY(_entity_) (!_entity_ ? nsnull : new bcEntity(_entity_))

class bcEntity : public Entity
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ENTITY
  NS_FORWARD_NODE(nodePtr->)

  bcEntity(nsIDOMEntity* domPtr);
  virtual ~bcEntity();
private:
  nsIDOMEntity* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcEntity_h__
