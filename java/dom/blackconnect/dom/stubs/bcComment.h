#ifndef __bcComment_h__
#define __bcComment_h__

#include "dom.h"
#include "nsIDOMComment.h"
#include "bcCharacterData.h"

#define DOM_COMMENT_PTR(_comment_) (!_comment_ ? nsnull : ((bcComment*)_comment_)->domPtr)
#define NEW_BCCOMMENT(_comment_) (!_comment_ ? nsnull : new bcComment(_comment_))

class bcComment : public Comment
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_COMMENT
  NS_FORWARD_NODE(dataPtr->)
  NS_FORWARD_CHARACTERDATA(dataPtr->)

  bcComment(nsIDOMComment* domPtr);
  virtual ~bcComment();
private:
  nsIDOMComment* domPtr;
  bcCharacterData* dataPtr;
  /* additional members */
};

#endif //  __bcComment_h__
