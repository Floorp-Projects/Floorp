/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class that represents a prefix/namespace/localName triple; a single
 * nodeinfo is shared by all elements in a document that have that
 * prefix, namespace, and localName.
 */

#include "mozilla/Util.h"

#include "nscore.h"
#include "nsNodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsDOMString.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsAutoPtr.h"
#include NEW_H
#include "nsFixedSizeAllocator.h"
#include "prprf.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"

using namespace mozilla;

static const size_t kNodeInfoPoolSizes[] = {
  sizeof(nsNodeInfo)
};

static const PRInt32 kNodeInfoPoolInitialSize = sizeof(nsNodeInfo) * 64;

// static
nsFixedSizeAllocator* nsNodeInfo::sNodeInfoPool = nsnull;

// static
nsNodeInfo*
nsNodeInfo::Create(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                   PRUint16 aNodeType, nsIAtom *aExtraName,
                   nsNodeInfoManager *aOwnerManager)
{
  if (!sNodeInfoPool) {
    sNodeInfoPool = new nsFixedSizeAllocator();
    if (!sNodeInfoPool)
      return nsnull;

    nsresult rv = sNodeInfoPool->Init("NodeInfo Pool", kNodeInfoPoolSizes,
                                      1, kNodeInfoPoolInitialSize);
    if (NS_FAILED(rv)) {
      delete sNodeInfoPool;
      sNodeInfoPool = nsnull;
      return nsnull;
    }
  }

  // Create a new one
  void* place = sNodeInfoPool->Alloc(sizeof(nsNodeInfo));
  return place ?
    new (place) nsNodeInfo(aName, aPrefix, aNamespaceID, aNodeType, aExtraName,
                           aOwnerManager) :
    nsnull;
}

nsNodeInfo::~nsNodeInfo()
{
  mOwnerManager->RemoveNodeInfo(this);

  NS_RELEASE(mInner.mName);
  NS_IF_RELEASE(mInner.mPrefix);
  NS_IF_RELEASE(mInner.mExtraName);
  NS_RELEASE(mOwnerManager);
}


nsNodeInfo::nsNodeInfo(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                       PRUint16 aNodeType, nsIAtom* aExtraName,
                       nsNodeInfoManager *aOwnerManager)
{
  CHECK_VALID_NODEINFO(aNodeType, aName, aNamespaceID, aExtraName);
  NS_ABORT_IF_FALSE(aOwnerManager, "Invalid aOwnerManager");

  // Initialize mInner
  NS_ADDREF(mInner.mName = aName);
  NS_IF_ADDREF(mInner.mPrefix = aPrefix);
  mInner.mNamespaceID = aNamespaceID;
  mInner.mNodeType = aNodeType;
  NS_ADDREF(mOwnerManager = aOwnerManager);
  NS_IF_ADDREF(mInner.mExtraName = aExtraName);

  mDocument = aOwnerManager->GetDocument();

  // Now compute our cached members.

  // Qualified name.  If we have no prefix, use ToString on
  // mInner.mName so that we get to share its buffer.
  if (aPrefix) {
    mQualifiedName = nsDependentAtomString(mInner.mPrefix) +
                     NS_LITERAL_STRING(":") +
                     nsDependentAtomString(mInner.mName);
  } else {
    mInner.mName->ToString(mQualifiedName);
  }

  switch (aNodeType) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::ATTRIBUTE_NODE:
      // Correct the case for HTML
      if (aNodeType == nsIDOMNode::ELEMENT_NODE &&
          aNamespaceID == kNameSpaceID_XHTML && GetDocument() &&
          GetDocument()->IsHTML()) {
        nsContentUtils::ASCIIToUpper(mQualifiedName, mNodeName);
      } else {
        mNodeName = mQualifiedName;
      }
      mInner.mName->ToString(mLocalName);
      break;
    case nsIDOMNode::TEXT_NODE:
    case nsIDOMNode::CDATA_SECTION_NODE:
    case nsIDOMNode::COMMENT_NODE:
    case nsIDOMNode::DOCUMENT_NODE:
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      mInner.mName->ToString(mNodeName);
      SetDOMStringToNull(mLocalName);
      break;
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
      mInner.mExtraName->ToString(mNodeName);
      SetDOMStringToNull(mLocalName);
      break;
    default:
      NS_ABORT_IF_FALSE(aNodeType == PR_UINT16_MAX,
                        "Unknown node type");
  }
}


// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsNodeInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsNodeInfo)

static const char* kNSURIs[] = {
  " ([none])",
  " (xmlns)",
  " (xml)",
  " (xhtml)",
  " (XLink)",
  " (XSLT)",
  " (XBL)",
  " (MathML)",
  " (RDF)",
  " (XUL)"
};

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsNodeInfo)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    char name[72];
    PRUint32 nsid = tmp->NamespaceID();
    nsAtomCString localName(tmp->NameAtom());
    if (nsid < ArrayLength(kNSURIs)) {
      PR_snprintf(name, sizeof(name), "nsNodeInfo%s %s", kNSURIs[nsid],
                  localName.get());
    }
    else {
      PR_snprintf(name, sizeof(name), "nsNodeInfo %s", localName.get());
    }

    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), sizeof(nsNodeInfo), name);
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsNodeInfo, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mOwnerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsNodeInfo, LastRelease())
NS_INTERFACE_TABLE_HEAD(nsNodeInfo)
  NS_INTERFACE_TABLE1(nsNodeInfo, nsINodeInfo)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsNodeInfo)
NS_INTERFACE_MAP_END

// nsINodeInfo

nsresult
nsNodeInfo::GetNamespaceURI(nsAString& aNameSpaceURI) const
{
  nsresult rv = NS_OK;

  if (mInner.mNamespaceID > 0) {
    rv = nsContentUtils::NameSpaceManager()->GetNameSpaceURI(mInner.mNamespaceID,
                                                             aNameSpaceURI);
  } else {
    SetDOMStringToNull(aNameSpaceURI);
  }

  return rv;
}


bool
nsNodeInfo::NamespaceEquals(const nsAString& aNamespaceURI) const
{
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  return nsINodeInfo::NamespaceEquals(nsid);
}

// static
void
nsNodeInfo::ClearCache()
{
  // Clear our cache.
  delete sNodeInfoPool;
  sNodeInfoPool = nsnull;
}

void
nsNodeInfo::LastRelease()
{
  nsRefPtr<nsNodeInfoManager> kungFuDeathGrip = mOwnerManager;
  this->~nsNodeInfo();

  // The refcount balancing and destructor re-entrancy protection
  // code in Release() sets mRefCnt to 1 so we have to set it to 0
  // here to prevent leaks
  mRefCnt = 0;

  NS_ASSERTION(sNodeInfoPool, "No NodeInfoPool when deleting NodeInfo!!!");
  sNodeInfoPool->Free(this, sizeof(nsNodeInfo));
}
