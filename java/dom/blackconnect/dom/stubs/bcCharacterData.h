#ifndef __bcCharacterData_h__
#define __bcCharacterData_h__

#include "dom.h"
#include "bcNode.h"
#include "nsIDOMCharacterData.h"

#define DOM_CHARACTERDATA_PTR(_characterdata_) (!_characterdata_ ? nsnull : ((bcCharacterData*)_characterdata_)->domPtr)
#define NEW_BCCHARACTERDATA(_characterdata_) (!_characterdata_ ? nsnull : new bcCharacterData(_characterdata_))

class bcCharacterData : public CharacterData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_CHARACTERDATA
  NS_FORWARD_NODE(nodePtr->)

  bcCharacterData(nsIDOMCharacterData* domPtr);
  virtual ~bcCharacterData();
private:
  nsIDOMCharacterData* domPtr;
  bcNode* nodePtr;
  /* additional members */
};

#endif //  __bcCharacterData_h__
