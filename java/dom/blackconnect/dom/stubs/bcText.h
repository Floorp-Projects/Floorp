#ifndef __bcText_h__
#define __bcText_h__

#include "dom.h"
#include "nsIDOMText.h"
#include "bcCharacterData.h"

#define DOM_TEXT_PTR(_text_) (!_text_ ? nsnull : ((bcText*)_text_)->domPtr)
#define NEW_BCTEXT(_text_) (!_text_ ? nsnull : new bcText(_text_))

class bcText : public Text
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_TEXT
  NS_FORWARD_NODE(dataPtr->)
  NS_FORWARD_CHARACTERDATA(dataPtr->)

  bcText(nsIDOMText* domPtr);
  virtual ~bcText();
private:
  nsIDOMText* domPtr;
  bcCharacterData* dataPtr;
  /* additional members */
};

#endif //  __bcText_h__
