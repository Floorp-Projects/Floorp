#include "nsCOMPtr.h"
#include "nsIXBLBinding.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsSupportsArray.h"

#include "nsIXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
// None


class nsXBLBinding: public nsIXBLBinding
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult);
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding);

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent);
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent);

  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GenerateAnonymousContent(nsIContent* aBoundElement);
  NS_IMETHOD InstallEventHandlers(nsIContent* aBoundElement);

public:
  nsXBLBinding();
  virtual ~nsXBLBinding();

// Static members
  static PRUint32 gRefCnt;
  
  static nsIAtom* nsXBLBinding::kContentAtom;
  static nsIAtom* nsXBLBinding::kInterfaceAtom;
  static nsIAtom* nsXBLBinding::kHandlersAtom;
  static nsIAtom* nsXBLBinding::kExcludesAtom;
  static nsIAtom* nsXBLBinding::kInheritsAtom;

// Internal member functions
protected:
  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  PRBool IsInExcludesList(nsIAtom* aTag, const nsString& aList);

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsIContent> mBinding; // Strong. As long as we're around, the binding can't go away.
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsCOMPtr<nsIXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.

  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.
};

// Static initialization
PRUint32 nsXBLBinding::gRefCnt = 0;
  
nsIAtom* nsXBLBinding::kContentAtom = nsnull;
nsIAtom* nsXBLBinding::kInterfaceAtom = nsnull;
nsIAtom* nsXBLBinding::kHandlersAtom = nsnull;
nsIAtom* nsXBLBinding::kExcludesAtom = nsnull;
nsIAtom* nsXBLBinding::kInheritsAtom = nsnull;

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLBinding, nsIXBLBinding)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(void)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
    kContentAtom = NS_NewAtom("content");
    kInterfaceAtom = NS_NewAtom("interface");
    kHandlersAtom = NS_NewAtom("handlers");
    kExcludesAtom = NS_NewAtom("excludes");
    kInheritsAtom = NS_NewAtom("inherits");
  }
}

nsXBLBinding::~nsXBLBinding(void)
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kContentAtom);
    NS_RELEASE(kInterfaceAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kExcludesAtom);
    NS_RELEASE(kInheritsAtom);
  }
}

// nsIXBLBinding Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::GetBaseBinding(nsIXBLBinding** aResult)
{
  *aResult = mNextBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBaseBinding(nsIXBLBinding* aBinding)
{
  mNextBinding = aBinding; // Comptr handles rel/add
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetAnonymousContent(nsIContent** aResult)
{
  *aResult = mContent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetAnonymousContent(nsIContent* aParent)
{
  // First cache the element.
  mContent = aParent;

  // Now we need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document.
  nsCOMPtr<nsIDocument> doc;
  mBoundElement->GetDocument(*getter_AddRefs(doc));
  mContent->SetDocument(doc, PR_TRUE);

  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the bound element.
  PRInt32 childCount;
  mContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mContent->ChildAt(i, *getter_AddRefs(child));
    child->SetParent(mBoundElement);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = mBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GenerateAnonymousContent(nsIContent* aBoundElement)
{
  // Set our bound element.
  mBoundElement = aBoundElement;

  // Fetch the content element for this binding.
  nsCOMPtr<nsIContent> content;
  GetImmediateChild(kContentAtom, getter_AddRefs(content));

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      return mNextBinding->GenerateAnonymousContent(aBoundElement);
    else return NS_OK;
  }

  // Plan to build the content by default.
  PRBool buildContent = PR_TRUE;
  PRInt32 childCount;
  aBoundElement->ChildCount(childCount);
  if (childCount > 0) {
    // See if there's an excludes attribute.
    // We'll only build content if all the explicit children are 
    // in the excludes list.
    nsAutoString excludes;
    content->GetAttribute(kNameSpaceID_None, kExcludesAtom, excludes);
    if (excludes == "true") {
      // Walk the children and ensure that all of them
      // are in the excludes array.
      for (PRInt32 i = 0; i < childCount; i++) {
        nsCOMPtr<nsIContent> child;
        aBoundElement->ChildAt(i, *getter_AddRefs(child));
        nsCOMPtr<nsIAtom> tag;
        child->GetTag(*getter_AddRefs(tag));
        if (!IsInExcludesList(tag, excludes)) {
          buildContent = PR_FALSE;
          break;
        }
      }
    }
    else buildContent = PR_FALSE;
  }
  
  if (buildContent) {
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(content);

    nsCOMPtr<nsIDOMNode> clonedNode;
    domElement->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
    
    nsCOMPtr<nsIContent> clonedContent = do_QueryInterface(clonedNode);
    SetAnonymousContent(clonedContent);
  }
  
  if (mNextBinding) {
    return mNextBinding->GenerateAnonymousContent(aBoundElement);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers(nsIContent* aBoundElement)
{
  // XXX Implement me!
  return NS_OK;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

void
nsXBLBinding::GetImmediateChild(nsIAtom* aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  mBinding->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mBinding->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}


PRBool
nsXBLBinding::IsInExcludesList(nsIAtom* aTag, const nsString& aList) 
{ 
  nsAutoString element;
  aTag->ToString(element);

  if (aList == "*")
      return PR_TRUE; // match _everything_!

  PRInt32 indx = aList.Find(element);
  if (indx == -1)
    return PR_FALSE; // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    PRUnichar ch = aList[indx - 1];
    if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  if (indx + element.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + element.Length()];
    if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  return PR_TRUE;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLBinding(nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}