#include "nsXFormsMDGSet.h"

MOZ_DECL_CTOR_COUNTER(nsXFormsMDGSet)

nsXFormsMDGSet::nsXFormsMDGSet()
{
  MOZ_COUNT_CTOR(nsXFormsMDGSet);
}

nsXFormsMDGSet::~nsXFormsMDGSet() {
  Clear();

  MOZ_COUNT_DTOR(nsXFormsMDGSet);
}

int
nsXFormsMDGSet::sortFunc(const void* aElement1, const void* aElement2, void* aData)
{
  int res = 0;
  if (aElement1 < aElement2)
    res = -1;
  if (aElement1 > aElement2)
    res = 1;

  return res; 
}


void
nsXFormsMDGSet::Clear()
{
  // Remove reference counters
  nsISupports* node;
  for (PRInt32 i = 0; i < array.Count(); ++i) {
    node = NS_STATIC_CAST(nsISupports*, array[i]);
    node->Release();
  }
  // Clear array
  array.Clear();
}

void
nsXFormsMDGSet::MakeUnique()
{
  array.Sort(sortFunc, nsnull);

  PRInt32 pos = 0;
  nsIDOMNode* node;
  while (pos + 1 < array.Count()) {
    if (array[pos] == array[pos + 1]) {
      node = NS_STATIC_CAST(nsIDOMNode*, array[pos + 1]);
      node->Release();
      array.RemoveElementAt(pos + 1);
    } else {
      ++pos;
    }
  }
}

PRInt32
nsXFormsMDGSet::Count() const
{
  return array.Count();
}

PRBool
nsXFormsMDGSet::AddNode(nsIDOMNode* aDomNode)
{
  if (!aDomNode) {
    return PR_FALSE;
  }
  
  aDomNode->AddRef();
  return array.AppendElement(aDomNode);
}

PRBool
nsXFormsMDGSet::AddSet(nsXFormsMDGSet& aSet)
{
  nsIDOMNode* node;
  for (PRInt32 i = 0; i < aSet.array.Count(); ++i) {
    node = NS_STATIC_CAST(nsIDOMNode*, aSet.array[i]);
    if (!AddNode(node)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}


///
/// @todo Is in fact a getter, should addref? (XXX)
/// AddRef it, and use already_AddRefed<nsIDOMNode>
nsIDOMNode*
nsXFormsMDGSet::GetNode(const PRInt32 aIndex) const {
  return NS_STATIC_CAST(nsIDOMNode*, array[aIndex]);
}
