#ifndef NSFRAMETRAVERSAL_H
#define NSFRAMETRAVERSAL_H

#include "nsIEnumerator.h"
#include "nsIFrame.h"

enum nsTraversalType{LEAF, EXTENSIVE, FASTEST}; 
nsresult NS_NewFrameTraversal(nsIEnumerator **aEnumerator, nsTraversalType aType, nsIFrame *aStart);


#endif //NSFRAMETRAVERSAL_H