/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Churchill <rjc@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsContentCID.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULDocument.h"
#include "nsIElementFactory.h"
#include "nsINodeInfo.h"
#include "nsIPrincipal.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIXULContent.h"
#include "nsIXULDocument.h"
#include "nsIXULSortService.h"

#include "nsClusterKeySet.h"
#include "nsContentSupportMap.h"
#include "nsContentTestNode.h"
#include "nsContentTagTestNode.h"
#include "nsInstantiationNode.h"
#include "nsRDFConMemberTestNode.h"
#include "nsRDFPropertyTestNode.h"
#include "nsRDFSort.h"
#include "nsTemplateRule.h"
#include "nsTemplateMap.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXULAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULElement.h"
#include "nsXULTemplateBuilder.h"

#include "jsapi.h"
#include "pldhash.h"
#include "rdf.h"

//----------------------------------------------------------------------0

static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,      NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);

//----------------------------------------------------------------------
//
// Return values for EnsureElementHasGenericChild()
//
#define NS_RDF_ELEMENT_GOT_CREATED NS_RDF_NO_VALUE
#define NS_RDF_ELEMENT_WAS_THERE   NS_OK

//----------------------------------------------------------------------
//
// nsXULContentBuilder
//

class nsXULContentBuilder : public nsXULTemplateBuilder,
                            public nsIRDFContentModelBuilder
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    // nsIXULTemplateBuilder interface
    NS_IMETHOD Rebuild();

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);

    // nsIDocumentObserver interface
    NS_IMETHOD AttributeChanged(nsIDocument* aDocument,
                                nsIContent*  aContent,
                                PRInt32      aNameSpaceID,
                                nsIAtom*     aAttribute,
                                PRInt32      aModType, 
                                PRInt32      aHint);

    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument* aDocument);

protected:
    friend NS_IMETHODIMP
    NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULContentBuilder();
    virtual ~nsXULContentBuilder();
    nsresult Init();

    // Implementation methods
    nsresult
    OpenContainer(nsIContent* aElement);

    nsresult
    CloseContainer(nsIContent* aElement);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aResourceNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aChild,
                             PRBool aNotify,
                             nsTemplateMatch* aMatch,
                             nsIContent** aContainer,
                             PRInt32* aNewIndexInContainer);

    nsresult
    AddPersistentAttributes(nsIContent* aTemplateNode, nsIRDFResource* aResource, nsIContent* aRealNode);

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             nsTemplateMatch& aMatch,
                             const VariableSet& aModifiedVars);

    PRBool
    IsDirectlyContainedBy(nsIContent* aChild, nsIContent* aParent);

    nsresult
    RemoveMember(nsIContent* aContainerElement,
                 nsIRDFResource* aMember,
                 PRBool aNotify);

    nsresult
    CreateTemplateAndContainerContents(nsIContent* aElement,
                                       nsIContent** aContainer,
                                       PRInt32* aNewIndexInContainer);

    nsresult
    CreateContainerContents(nsIContent* aElement,
                            nsIRDFResource* aResource,
                            PRBool aNotify,
                            nsIContent** aContainer,
                            PRInt32* aNewIndexInContainer);

    nsresult
    CreateTemplateContents(nsIContent* aElement,
                           nsIContent* aTemplateElement,
                           nsIContent** aContainer,
                           PRInt32* aNewIndexInContainer);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 PRBool aNotify,
                                 nsIContent** aResult);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    nsresult
    RemoveGeneratedContent(nsIContent* aElement);

    PRBool
    IsLazyWidgetItem(nsIContent* aElement);

    static void
    GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

    nsresult
    GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);

    nsresult
    CreateElement(PRInt32 aNameSpaceID,
                  nsIAtom* aTag,
                  nsIContent** aResult);

    nsresult
    SetContainerAttrs(nsIContent *aElement, const nsTemplateMatch* aMatch);

    virtual nsresult
    InitializeRuleNetwork();

    virtual nsresult
    InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode);

    virtual nsresult
    CompileCondition(nsIAtom* aTag,
                     nsTemplateRule* aRule,
                     nsIContent* aCondition,
                     InnerNode* aParentNode,
                     TestNode** aResult);

    nsresult
    CompileContentCondition(nsTemplateRule* aRule,
                            nsIContent* aCondition,
                            InnerNode* aParentNode,
                            TestNode** aResult);

    /**
     * Override default implementation to provide for ``parent'' test
     */
    virtual PRBool
    CompileSimpleAttributeCondition(PRInt32 aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    const nsAReadableString& aValue,
                                    InnerNode* aParentNode,
                                    TestNode** aResult);
    /**
     * Maintains a mapping between elements in the DOM and the matches
     * in the conflict set that they support.
     */
    nsContentSupportMap mContentSupportMap;

    /**
     * The variable that represents the ``resource element'' in the
     * rules for this builder.
     */
    PRInt32 mContentVar;

    /**
     * Maintains a mapping from an element in the DOM to the template
     * element that it was created from.
     */
    nsTemplateMap mTemplateMap;

    /**
     * Information about the currently active sort
     */
    nsRDFSortState sortState;

    nsresult
    Rebuild(nsIContent* aElement);

    virtual nsresult
    ReplaceMatch(nsIRDFResource* aMember, const nsTemplateMatch* aOldMatch, nsTemplateMatch* aNewMatch);

    virtual nsresult
    SynchronizeMatch(nsTemplateMatch* aMatch, const VariableSet& aModifiedVars);

    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIElementFactory* gHTMLElementFactory;
    static nsIElementFactory* gXMLElementFactory;
    static nsIXULSortService* gXULSortService;
};

PRInt32             nsXULContentBuilder::gRefCnt;
nsIXULSortService*  nsXULContentBuilder::gXULSortService;
nsIElementFactory*  nsXULContentBuilder::gHTMLElementFactory;
nsIElementFactory*  nsXULContentBuilder::gXMLElementFactory;

NS_IMPL_ISUPPORTS_INHERITED1(nsXULContentBuilder, nsXULTemplateBuilder,
                             nsIRDFContentModelBuilder)

NS_IMETHODIMP
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsresult rv;
    nsXULContentBuilder* result = new nsXULContentBuilder();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();

    if (NS_SUCCEEDED(rv))
        rv = result->QueryInterface(aIID, aResult);

    NS_RELEASE(result);
    return rv;
}

nsXULContentBuilder::nsXULContentBuilder()
{
}

nsXULContentBuilder::~nsXULContentBuilder()
{
    if (--gRefCnt == 0) {
        if (gXULSortService) {
            nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
            gXULSortService = nsnull;
        }

        NS_IF_RELEASE(gHTMLElementFactory);
        NS_IF_RELEASE(gXMLElementFactory);
    }
}

nsresult
nsXULContentBuilder::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = nsServiceManager::GetService(kXULSortServiceCID,
                                          NS_GET_IID(nsIXULSortService),
                                          (nsISupports**) &gXULSortService);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gHTMLElementFactory);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gXMLElementFactory);
        if (NS_FAILED(rv)) return rv;
    }

    return nsXULTemplateBuilder::Init();
}

PRBool
nsXULContentBuilder::IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // XXX Note that we patently ignore namespaces. This is because
    // HTML elements lie and tell us that their attributes are
    // _always_ in the HTML namespace. Urgh.

    // never copy the ID attribute
    if (aAttribute == nsXULAtoms::id) {
        return PR_TRUE;
    }
    // never copy {}:uri attribute
    else if (aAttribute == nsXULAtoms::uri) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

nsresult
nsXULContentBuilder::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                              nsIContent *aResourceNode,
                                              nsIContent *aRealNode,
                                              PRBool aIsUnique,
                                              nsIRDFResource* aChild,
                                              PRBool aNotify,
                                              nsTemplateMatch* aMatch,
                                              nsIContent** aContainer,
                                              PRInt32* aNewIndexInContainer)
{
    // This is the mother lode. Here is where we grovel through an
    // element in the template, copying children from the template
    // into the "real" content tree, performing substitution as we go
    // by looking stuff up in the RDF graph.
    //
    //   |aTemplateNode| is the element in the "template tree", whose
    //   children we will duplicate and move into the "real" content
    //   tree.
    //
    //   |aResourceNode| is the element in the "real" content tree that
    //   has the "id" attribute set to an RDF resource's URI. This is
    //   not directly used here, but rather passed down to the XUL
    //   sort service to perform container-level sort.
    //
    //   |aRealNode| is the element in the "real" content tree to which
    //   the new elements will be copied.
    //
    //   |aIsUnique| is set to "true" so long as content has been
    //   "unique" (or "above" the resource element) so far in the
    //   template.
    //
    //   |aChild| is the RDF resource at the end of a property link for
    //   which we are building content.
    //
    //   |aNotify| is set to "true" if content should be constructed
    //   "noisily"; that is, whether the document observers should be
    //   notified when new content is added to the content model.
    //
    //   |aContainer| is an out parameter that will be set to the first
    //   container element in the "real" content tree to which content
    //   was appended.
    //
    //   |aNewIndexInContainer| is an out parameter that will be set to
    //   the index in aContainer at which new content is first
    //   constructed.
    //
    // If |aNotify| is "false", then |aContainer| and
    // |aNewIndexInContainer| are used to determine where in the
    // content model new content is constructed. This allows a single
    // notification to be propogated to document observers.
    //

    nsresult rv;

#ifdef PR_LOGGING
    // Dump out the template node's tag, the template ID, and the RDF
    // resource that is being used as the index into the graph.
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString resourceCStr;
        rv = aChild->GetValue(getter_Copies(resourceCStr));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagstr;
        tag->ToString(tagstr);

        nsAutoString templatestr;
        aTemplateNode->GetAttr(kNameSpaceID_None, nsXULAtoms::id, templatestr);
        nsCAutoString templatestrC,tagstrC;
        tagstrC.AssignWithConversion(tagstr);
        templatestrC.AssignWithConversion(templatestr);
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] build-content-from-template %s (template='%s') [%s]",
                this,
                (const char*) tagstrC,
                (const char*) templatestrC,
                (const char*) resourceCStr));
    }
#endif

    // Iterate through all of the template children, constructing
    // "real" content model nodes for each "template" child.
    PRInt32    count;
    rv = aTemplateNode->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 kid = 0; kid < count; kid++) {
        nsCOMPtr<nsIContent> tmplKid;
        rv = aTemplateNode->ChildAt(kid, *getter_AddRefs(tmplKid));
        if (NS_FAILED(rv)) return rv;

        PRInt32 nameSpaceID;
        rv = tmplKid->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        // Check whether this element is the "resource" element. The
        // "resource" element is the element that is cookie-cutter
        // copied once for each different RDF resource specified by
        // |aChild|.
        //
        // Nodes that appear -above- the resource element
        // (that is, are ancestors of the resource element in the
        // content model) are unique across all values of |aChild|,
        // and are created only once.
        //
        // Nodes that appear -below- the resource element (that is,
        // are descnendants of the resource element in the conte
        // model), are cookie-cutter copied for each distinct value of
        // |aChild|.
        //
        // For example, in a <tree> template:
        //
        //   <tree>
        //     <template>
        //       <treechildren> [1]
        //         <treeitem uri="rdf:*"> [2]
        //           <treerow> [3]
        //             <treecell value="rdf:urn:foo" /> [4]
        //             <treecell value="rdf:urn:bar" /> [5]
        //           </treerow>
        //         </treeitem>
        //       </treechildren>
        //     </template>
        //   </tree>
        //
        // The <treeitem> element [2] is the "resource element". This
        // element, and all of its descendants ([3], [4], and [5])
        // will be duplicated for each different |aChild|
        // resource. It's ancestor <treechildren> [1] is unique, and
        // will only be created -once-, no matter how many <treeitem>s
        // are created below it.
        //
        // Note that |isResourceElement| and |isUnique| are mutually
        // exclusive.
        PRBool isResourceElement = PR_FALSE;
        PRBool isUnique = aIsUnique;

        {
            // We identify the resource element by presence of a
            // "uri='rdf:*'" attribute. (We also support the older
            // "uri='...'" syntax.)
            nsAutoString uri;
            tmplKid->GetAttr(kNameSpaceID_None, nsXULAtoms::uri, uri);

            if ( !uri.IsEmpty() ) {
              if (aMatch->mRule && uri[0] == PRUnichar('?')) {
                  isResourceElement = PR_TRUE;
                  isUnique = PR_FALSE;

                  // XXXwaterson hack! refactor me please
                  Value member;
                  aMatch->mAssignments.GetAssignmentFor(aMatch->mRule->GetMemberVariable(), &member);
                  aChild = VALUE_TO_IRDFRESOURCE(member);
              }
              else if (uri.EqualsWithConversion("...") || uri.EqualsWithConversion("rdf:*")) {
                  // If we -are- the resource element, then we are no
                  // matter unique.
                  isResourceElement = PR_TRUE;
                  isUnique = PR_FALSE;
              }
            }
        }

        nsCOMPtr<nsIAtom> tag;
        rv = tmplKid->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            nsAutoString tagname;
            tag->ToString(tagname);
            nsCAutoString tagstrC;
            tagstrC.AssignWithConversion(tagname);
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p]     building %s %s %s",
                    this, (const char*) tagstrC,
                    (isResourceElement ? "[resource]" : ""),
                    (isUnique ? "[unique]" : "")));
        }
#endif

        // Set to PR_TRUE if the child we're trying to create now
        // already existed in the content model.
        PRBool realKidAlreadyExisted = PR_FALSE;

        nsCOMPtr<nsIContent> realKid;
        if (isUnique) {
            // The content is "unique"; that is, we haven't descended
            // far enough into the tempalte to hit the "resource"
            // element yet. |EnsureElementHasGenericChild()| will
            // conditionally create the element iff it isn't there
            // already.
            rv = EnsureElementHasGenericChild(aRealNode, nameSpaceID, tag, aNotify, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_RDF_ELEMENT_WAS_THERE) {
                realKidAlreadyExisted = PR_TRUE;
            }
            else {
                // Mark the element's contents as being generated so
                // that any re-entrant calls don't trigger an infinite
                // recursion.
                nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
                if (xulcontent) {
                    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
                    if (NS_FAILED(rv)) return rv;
                }

                // Potentially remember the index of this element as the first
                // element that we've generated. Note that we remember
                // this -before- we recurse!
                if (aContainer && !*aContainer) {
                    *aContainer = aRealNode;
                    NS_ADDREF(*aContainer);

					PRInt32 indx;
                    aRealNode->ChildCount(indx);

					// Since EnsureElementHasGenericChild() added us, make
					// sure to subtract one for our real index.
					*aNewIndexInContainer = indx - 1;
                }
            }

            // Recurse until we get to the resource element. Since
            // -we're- unique, assume that our child will be
            // unique. The check for the "resource" element at the top
            // of the function will trip this to |false| as soon as we
            // encounter it.
            rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, PR_TRUE,
                                          aChild, aNotify, aMatch,
                                          aContainer, aNewIndexInContainer);

            if (NS_FAILED(rv)) return rv;
        }
        else if (isResourceElement) {
            // It's the "resource" element. Create a new element using
            // the namespace ID and tag from the template element.
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            // Add the resource element to the content support map so
            // we can the match based on content node later.
            mContentSupportMap.Put(realKid, aMatch);

            // Assign the element an 'id' attribute using the URI of
            // the |aChild| resource.
            const char *uri;
            rv = aChild->GetValueConst(&uri);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
            if (NS_FAILED(rv)) return rv;

            // XXX because gcc-2.7.2.3 is too dumb to keep a
            // compiler-generated temporary around.
            NS_ConvertUTF8toUCS2 x(uri);
            const nsAReadableString& id = x;
            rv = realKid->SetAttr(kNameSpaceID_None, nsXULAtoms::id, id, PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set id attribute");
            if (NS_FAILED(rv)) return rv;

            if (! aNotify) {
                // XUL document will watch us, and take care of making
                // sure that we get added to or removed from the
                // element map if aNotify is true. If not, we gotta do
                // it ourselves. Yay.
                nsCOMPtr<nsIDocument> doc;
                mRoot->GetDocument(*getter_AddRefs(doc));

                if (doc) {
                    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
                    if (xuldoc)
                        xuldoc->AddElementForID(id, realKid);
                }
            }

            // XXX Hackery to ensure that mailnews works. Force the
            // element to hold a reference to the
            // resource. Unfortunately, this'll break for HTML
            // elements.
            {
                nsCOMPtr<nsIXULContent> xulele = do_QueryInterface(realKid);
                if (xulele) {
                    xulele->ForceElementToOwnResource(PR_TRUE);
                }
            }

            // Set up the element's 'container' and 'empty'
            // attributes.
            PRBool iscontainer, isempty;
            rv = CheckContainer(aChild, &iscontainer, &isempty);
            if (NS_FAILED(rv)) return rv;

            if (iscontainer) {
                realKid->SetAttr(kNameSpaceID_None, nsXULAtoms::container,
                                 NS_LITERAL_STRING("true"), PR_FALSE);

                if (! (mFlags & eDontTestEmpty)) {
                    NS_NAMED_LITERAL_STRING(true_, "true");
                    NS_NAMED_LITERAL_STRING(false_, "false");

                    realKid->SetAttr(kNameSpaceID_None, nsXULAtoms::empty,
                                     isempty ? true_ : false_,
                                     PR_FALSE);
                }
            }
        }
        else if ((tag.get() == nsXULAtoms::textnode) && (nameSpaceID == kNameSpaceID_XUL)) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the 'rdf:resource' attribute for the
            // given node.
            PRUnichar attrbuf[128];
            nsAutoString attrValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
            rv = tmplKid->GetAttr(kNameSpaceID_None, nsXULAtoms::value, attrValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.Length() > 0)) {
                nsAutoString value;
                rv = SubstituteText(*aMatch, attrValue, value);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsITextContent> content;
                rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        NS_GET_IID(nsITextContent),
                                                        getter_AddRefs(content));
                if (NS_FAILED(rv)) return rv;

                rv = content->SetText(value.get(), value.Length(), PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                rv = aRealNode->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(content) ),
                                              aNotify, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // XXX Don't bother remembering text nodes as the
                // first element we've generated?
            }
        }
        else {
            // It's just a generic element. Create it!
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;
        }

        if (realKid && !realKidAlreadyExisted) {
            // Potentially remember the index of this element as the
            // first element that we've generated.
            if (aContainer && !*aContainer) {
                *aContainer = aRealNode;
                NS_ADDREF(*aContainer);

                PRInt32 indx;
                aRealNode->ChildCount(indx);

                // Since we haven't inserted any content yet, our new
                // index in the container will be the current count of
                // elements in the container.
                *aNewIndexInContainer = indx;
            }

            // Remember the template kid from which we created the
            // real kid. This allows us to sync back up with the
            // template to incrementally build content.
            mTemplateMap.Put(realKid, tmplKid);

            // Copy all attributes from the template to the new
            // element.
            PRInt32    numAttribs;
            rv = tmplKid->GetAttrCount(numAttribs);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 attr = 0; attr < numAttribs; attr++) {
                PRInt32 attribNameSpaceID;
                nsCOMPtr<nsIAtom> attribName, prefix;

                rv = tmplKid->GetAttrNameAt(attr, attribNameSpaceID, *getter_AddRefs(attribName), *getter_AddRefs(prefix));
                if (NS_FAILED(rv)) return rv;

                if (! IsIgnoreableAttribute(attribNameSpaceID, attribName)) {
                    // Create a buffer here, because there's a good
                    // chance that an attribute in the template is
                    // going to be an RDF URI, which is usually
                    // longish.
                    PRUnichar attrbuf[128];
                    nsAutoString attribValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
                    rv = tmplKid->GetAttr(attribNameSpaceID, attribName, attribValue);
                    if (NS_FAILED(rv)) return rv;

                    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        nsAutoString value;
                        rv = SubstituteText(*aMatch, attribValue, value);
                        if (NS_FAILED(rv)) return rv;

                        rv = realKid->SetAttr(attribNameSpaceID, attribName, value, PR_FALSE);
                        if (NS_FAILED(rv)) return rv;
                    }
                }
            }

            // Add any persistent attributes
            if (isResourceElement) {
                rv = AddPersistentAttributes(tmplKid, aChild, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            
            nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
            if (xulcontent) {
                PRInt32 count2;
                tmplKid->ChildCount(count2);

                if (count2 == 0 && !isResourceElement) {
                    // If we're at a leaf node, then we'll eagerly
                    // mark the content as having its template &
                    // container contents built. This avoids a useless
                    // trip back to the template builder only to find
                    // that we've got no work to do!
                    xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
                    xulcontent->SetLazyState(nsIXULContent::eContainerContentsBuilt);
                }
                else {
                    // Just mark the XUL element as requiring more work to
                    // be done. We'll get around to it when somebody asks
                    // for it.
                    xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
                }
            }
            else {
                // Otherwise, it doesn't support lazy instantiation,
                // and we have to recurse "by hand". Note that we
                // _don't_ need to notify: we'll add the entire
                // subtree in a single whack.
                //
                // Note that we don't bother passing aContainer and
                // aNewIndexInContainer down: since we're HTML, we
                // -know- that we -must- have just been created.
                rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, isUnique,
                                              aChild, PR_FALSE, aMatch,
                                              nsnull /* don't care */,
                                              nsnull /* don't care */);

                if (NS_FAILED(rv)) return rv;

                if (isResourceElement) {
                    rv = CreateContainerContents(realKid, aChild, PR_FALSE,
                                                 nsnull /* don't care */,
                                                 nsnull /* don't care */);
                    if (NS_FAILED(rv)) return rv;
                }
            }

            // We'll _already_ have added the unique elements; but if
            // it's -not- unique, then use the XUL sort service now to
            // append the element to the content model.
            if (! isUnique) {
                rv = NS_ERROR_UNEXPECTED;

                if (gXULSortService && isResourceElement) {
                    rv = gXULSortService->InsertContainerNode(mDB, &sortState,
                                                              mRoot, aResourceNode,
                                                              aRealNode, realKid,
                                                              aNotify);
                }

                if (NS_FAILED(rv)) {
                    rv = aRealNode->AppendChildTo(realKid, aNotify, PR_FALSE);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element");
                }
            }
        }
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::AddPersistentAttributes(nsIContent* aTemplateNode,
                                             nsIRDFResource* aResource,
                                             nsIContent* aRealNode)
{
    nsresult rv;

    nsAutoString persist;
    rv = aTemplateNode->GetAttr(kNameSpaceID_None, nsXULAtoms::persist, persist);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_OK;

    nsAutoString attribute;
    while (persist.Length() > 0) {
        attribute.Truncate();

        PRInt32 offset = persist.FindCharInSet(" ,");
        if (offset > 0) {
            persist.Left(attribute, offset);
            persist.Cut(0, offset + 1);
        }
        else {
            attribute = persist;
            persist.Truncate();
        }

        attribute.Trim(" ");

        if (attribute.Length() == 0)
            break;

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> tag;
        nsCOMPtr<nsINodeInfo> ni;
        rv = aTemplateNode->NormalizeAttrString(attribute,
                                                *getter_AddRefs(ni));
        if (NS_FAILED(rv)) return rv;

        ni->GetNameAtom(*getter_AddRefs(tag));
        ni->GetNamespaceID(nameSpaceID);

        nsCOMPtr<nsIRDFResource> property;
        rv = nsXULContentUtils::GetResource(nameSpaceID, tag, getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> target;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(target));
        if (NS_FAILED(rv)) return rv;

        if (! target)
            continue;

        nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(target);
        NS_ASSERTION(value != nsnull, "unable to stomach that sort of node");
        if (! value)
            continue;

        const PRUnichar* valueStr;
        rv = value->GetValueConst(&valueStr);
        if (NS_FAILED(rv)) return rv;

        rv = aRealNode->SetAttr(nameSpaceID, tag, nsAutoString(valueStr), PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::SynchronizeUsingTemplate(nsIContent* aTemplateNode,
                                              nsIContent* aRealElement,
                                              nsTemplateMatch& aMatch,
                                              const VariableSet& aModifiedVars)
{
    nsresult rv;

    // check all attributes on the template node; if they reference a resource,
    // update the equivalent attribute on the content node

    PRInt32    numAttribs;
    rv = aTemplateNode->GetAttrCount(numAttribs);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        for (PRInt32 aLoop=0; aLoop<numAttribs; aLoop++) {
            PRInt32    attribNameSpaceID;
            nsCOMPtr<nsIAtom> attribName, prefix;
            rv = aTemplateNode->GetAttrNameAt(aLoop,
                                              attribNameSpaceID,
                                              *getter_AddRefs(attribName),
                                              *getter_AddRefs(prefix));
            if (NS_FAILED(rv)) break;

            // See if it's one of the attributes that we unilaterally
            // ignore. If so, on to the next one...
            if (IsIgnoreableAttribute(attribNameSpaceID, attribName))
                continue;

            nsAutoString attribValue;
            rv = aTemplateNode->GetAttr(attribNameSpaceID,
                                        attribName,
                                        attribValue);

            if (! IsAttrImpactedByVars(aMatch, attribValue, aModifiedVars))
                continue;

            nsAutoString newvalue;
            SubstituteText(aMatch, attribValue, newvalue);

            if (newvalue.Length() > 0) {
                aRealElement->SetAttr(attribNameSpaceID,
                                      attribName,
                                      newvalue,
                                      PR_TRUE);
            }
            else {
                aRealElement->UnsetAttr(attribNameSpaceID,
                                        attribName,
                                        PR_TRUE);
            }
        }
    }

    // See if we've generated kids for this node yet. If we have, then
    // recursively sync up template kids with content kids
    PRBool contentsGenerated = PR_TRUE;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aRealElement);
    if (xulcontent) {
        rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt,
                                      contentsGenerated);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // HTML content will _always_ have been generated up-front
    }

    if (contentsGenerated) {
        PRInt32 count;
        rv = aTemplateNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 loop=0; loop<count; loop++) {
            nsCOMPtr<nsIContent> tmplKid;
            rv = aTemplateNode->ChildAt(loop, *getter_AddRefs(tmplKid));
            if (NS_FAILED(rv)) return rv;

            if (! tmplKid)
                break;

            nsCOMPtr<nsIContent> realKid;
            rv = aRealElement->ChildAt(loop, *getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (! realKid)
                break;

            rv = SynchronizeUsingTemplate(tmplKid, realKid, aMatch, aModifiedVars);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


PRBool
nsXULContentBuilder::IsDirectlyContainedBy(nsIContent* aChild, nsIContent* aParent)
{
    // This routine uses the <template> to determine if aChild is
    // "directly contained by" aParent. It does so by walking up the
    // template subtree in parallel with the generated subtree.
    NS_PRECONDITION(aChild != nsnull, "null ptr");
    if (! aChild)
        return PR_FALSE;

    // First, we need to find the template from which this element was
    // generated.
    nsCOMPtr<nsIContent> tmpl;
    mTemplateMap.GetTemplateFor(aChild, getter_AddRefs(tmpl));
    if (! tmpl)
        return PR_FALSE;

    // Now walk up the template subtree in parallel with the generated
    // subtree.
    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsIContent> generated(aChild);

    do {
        // XXX - gcc 2.95.x -O3 builds are known to break if
        // we declare nsCOMPtrs inside this loop.  Moving them
        // out of the loop or using a normal pointer works
        // around this problem.
        // http://bugzilla.mozilla.org/show_bug.cgi?id=61501

        // Walk up the generated tree
        nsIContent *tmp = generated;
        tmp->GetParent(*getter_AddRefs(generated));
        if (! generated) 
            return PR_FALSE;

        // Walk up the template tree
        tmp = tmpl;
        tmp->GetParent(*getter_AddRefs(tmpl));
        if (! tmpl)
            return PR_FALSE;

        // The content within a template ends when we hit the
        // <template> or <rule> element in the simple syntax, or the
        // <action> element in the extended syntax.
        tmpl->GetTag(*getter_AddRefs(tag));
    } while (tag.get() != nsXULAtoms::templateAtom &&
             tag.get() != nsXULAtoms::rule &&
             tag.get() != nsXULAtoms::action);

    // Did we find the generated parent?
    return PRBool(generated.get() == aParent);
}


nsresult
nsXULContentBuilder::RemoveMember(nsIContent* aContainerElement,
                                  nsIRDFResource* aMember,
                                  PRBool aNotify)
{
    // This works as follows. It finds all of the elements in the
    // document that correspond to aMember. Any that are contained
    // within aContainerElement are removed from their direct parent.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(aMember, elements);
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> child( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        if (! IsDirectlyContainedBy(child, aContainerElement))
            continue;

        nsCOMPtr<nsIContent> parent;
        rv = child->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        PRInt32 pos;
        rv = parent->IndexOf(child, pos);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) continue;

        rv = parent->RemoveChildAt(pos, aNotify);
        if (NS_FAILED(rv)) return rv;

        // Set its document to null so that it'll get knocked out of
        // the XUL doc's resource-to-element map.
        rv = child->SetDocument(nsnull, PR_TRUE, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        // Remove from the content support map.
        mContentSupportMap.Remove(child);

        // Remove from the template map
        mTemplateMap.Remove(child);

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_ALWAYS)) {
            nsCOMPtr<nsIAtom> parentTag;
            rv = parent->GetTag(*getter_AddRefs(parentTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString parentTagStr;
            rv = parentTag->ToString(parentTagStr);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIAtom> childTag;
            rv = child->GetTag(*getter_AddRefs(childTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString childTagStr;
            rv = childTag->ToString(childTagStr);
            if (NS_FAILED(rv)) return rv;

            const char* resourceCStr;
            rv = aMember->GetValueConst(&resourceCStr);
            if (NS_FAILED(rv)) return rv;
            
            nsCAutoString childtagstrC,parenttagstrC;
            parenttagstrC.AssignWithConversion(parentTagStr);
            childtagstrC.AssignWithConversion(childTagStr);
            PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
                   ("xultemplate[%p] remove-member %s->%s [%s]",
                    this,
                    (const char*) parenttagstrC,
                    (const char*) childtagstrC,
                    resourceCStr));
        }
#endif
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::CreateTemplateAndContainerContents(nsIContent* aElement,
                                                        nsIContent** aContainer,
                                                        PRInt32* aNewIndexInContainer)
{
    // Generate both 1) the template content for the current element,
    // and 2) recursive subcontent (if the current element refers to a
    // container resource in the RDF graph).

    // If we're asked to return the first generated child, then
    // initialize to "none".
    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // Create the current resource's contents from the template, if
    // appropriate
    nsCOMPtr<nsIContent> tmpl;
    mTemplateMap.GetTemplateFor(aElement, getter_AddRefs(tmpl));

    if (tmpl)
        CreateTemplateContents(aElement, tmpl, aContainer, aNewIndexInContainer);

    nsCOMPtr<nsIRDFResource> resource;
    nsXULContentUtils::GetElementRefResource(aElement, getter_AddRefs(resource));
    if (resource) {
        // The element has a resource; that means that it corresponds
        // to something in the graph, so we need to go to the graph to
        // create its contents.
        CreateContainerContents(aElement, resource, PR_FALSE, aContainer, aNewIndexInContainer);
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::CreateContainerContents(nsIContent* aElement,
                                             nsIRDFResource* aResource,
                                             PRBool aNotify,
                                             nsIContent** aContainer,
                                             PRInt32* aNewIndexInContainer)
{
    // Create the contents of a container by iterating over all of the
    // "containment" arcs out of the element's resource.
    nsresult rv;

	// Compile the rules now, if they haven't been already.
    if (! mRulesCompiled) {
        rv = CompileRules();
        if (NS_FAILED(rv)) return rv;
    }
    
    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // The tree widget is special. If the item isn't open, then just
    // "pretend" that there aren't any contents here. We'll create
    // them when OpenContainer() gets called.
    if (IsLazyWidgetItem(aElement) && !IsOpen(aElement))
        return NS_OK;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (xulcontent) {
        PRBool contentsGenerated;
        rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
        if (NS_FAILED(rv)) return rv;

        if (contentsGenerated)
            return NS_OK;

        // Now mark the element's contents as being generated so that
        // any re-entrant calls don't trigger an infinite recursion.
        rv = xulcontent->SetLazyState(nsIXULContent::eContainerContentsBuilt);
    }
    else {
        // HTML is always needs to be generated.
        //
        // XXX Big ass-umption here -- I am assuming that this will
        // _only_ ever get called (in the case of an HTML element)
        // when the XUL builder is descending thru the graph and
        // stumbles on a template that is rooted at an HTML element.
        // (/me crosses fingers...)
    }

    // Seed the rule network with assignments for the content and
    // container variables
    //
    // XXXwaterson could this code be shared with
    // nsXULTemplateBuilder::Propogate()?
    Instantiation seed;
    seed.AddAssignment(mContentVar, Value(aElement));

    InstantiationSet instantiations;
    instantiations.Append(seed);

    // Propogate the assignments through the network
    nsClusterKeySet newkeys;
    mRules.GetRoot()->Propogate(instantiations, &newkeys);

    // Iterate through newly added keys to determine which rules fired
    nsClusterKeySet::ConstIterator last = newkeys.Last();
    for (nsClusterKeySet::ConstIterator key = newkeys.First(); key != last; ++key) {
        nsConflictSet::MatchCluster* matches =
            mConflictSet.GetMatchesForClusterKey(*key);

        if (! matches)
            continue;

        nsTemplateMatch* match = 
            mConflictSet.GetMatchWithHighestPriority(matches);

        NS_ASSERTION(match != nsnull, "no best match in match set");
        if (! match)
            continue;

        // Grab the template node
        nsCOMPtr<nsIContent> tmpl;
        match->mRule->GetContent(getter_AddRefs(tmpl));

        BuildContentFromTemplate(tmpl, aElement, aElement, PR_TRUE,
                                 VALUE_TO_IRDFRESOURCE(key->mMemberValue),
                                 aNotify, match,
                                 aContainer, aNewIndexInContainer);

        // Remember this as the "last" match
        matches->mLastMatch = match;
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::CreateTemplateContents(nsIContent* aElement,
                                            nsIContent* aTemplateElement,
                                            nsIContent** aContainer,
                                            PRInt32* aNewIndexInContainer)
{
    // Create the contents of an element using the templates
    nsresult rv;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (! xulcontent)
        return NS_OK; // HTML content is _always_ generated up-front

    PRBool contentsGenerated;
    rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt, contentsGenerated);
    if (NS_FAILED(rv)) return rv;

    if (contentsGenerated)
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set template-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    // Crawl up the content model until we find the "resource" element
    // that spawned this template.
    nsCOMPtr<nsIRDFResource> resource;

    nsCOMPtr<nsIContent> element = aElement;
    while (element) {
        nsXULContentUtils::GetElementRefResource(element, getter_AddRefs(resource));
        if (resource)
            break;

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));

        element = parent;
    }

    nsTemplateMatch* match;
    mContentSupportMap.Get(element, &match);

    rv = BuildContentFromTemplate(aTemplateElement, aElement, aElement, PR_FALSE, resource, PR_FALSE,
                                  match, aContainer, aNewIndexInContainer);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsXULContentBuilder::EnsureElementHasGenericChild(nsIContent* parent,
                                                  PRInt32 nameSpaceID,
                                                  nsIAtom* tag,
                                                  PRBool aNotify,
                                                  nsIContent** result)
{
    nsresult rv;

    rv = nsXULContentUtils::FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        rv = CreateElement(nameSpaceID, tag, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        // XXX Note that the notification ensures we won't batch insertions! This could be bad! - Dave
        rv = parent->AppendChildTo(element, aNotify, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        *result = element;
        NS_ADDREF(*result);
        return NS_RDF_ELEMENT_GOT_CREATED;
    }
    else {
        return NS_RDF_ELEMENT_WAS_THERE;
    }
}

PRBool
nsXULContentBuilder::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    // XXXhyatt - use XBL service to obtain base tag.

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    // Treat the 'root' element as always open, -unless- it's a
    // menu/menupopup. We don't need to "fake" these as being open.
    if ((aElement == mRoot) && (tag.get() != nsXULAtoms::menu) &&
        (tag.get() != nsXULAtoms::menubutton))
      return PR_TRUE;

    nsAutoString value;
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::open, value);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get open attribute");
    if (NS_FAILED(rv)) return PR_FALSE;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value == NS_LITERAL_STRING("true"))
            return PR_TRUE;
    }

    
    return PR_FALSE;
}


PRBool
nsXULContentBuilder::IsElementInWidget(nsIContent* aElement)
{
    return IsElementContainedBy(aElement, mRoot);
}

nsresult
nsXULContentBuilder::RemoveGeneratedContent(nsIContent* aElement)
{
    // Keep a queue of "ungenerated" elements that we have to probe
    // for generated content.
    nsAutoVoidArray ungenerated;
    ungenerated.AppendElement(aElement);

    PRInt32 count;
    while (0 != (count = ungenerated.Count())) {
        // Pull the next "ungenerated" element off the queue.
        PRInt32 last = count - 1;
        nsIContent* element = NS_STATIC_CAST(nsIContent*, ungenerated[last]);
        ungenerated.RemoveElementAt(last);

        PRInt32 i = 0;
        element->ChildCount(i);

        while (--i >= 0) {
            nsCOMPtr<nsIContent> child;
            element->ChildAt(i, *getter_AddRefs(child));
            NS_ASSERTION(child != nsnull, "huh? no child?");
            if (! child)
                continue;

            // Optimize for the <template> element, because we *know*
            // it won't have any generated content: there's no reason
            // to even check this subtree.
            nsCOMPtr<nsIAtom> tag;
            element->GetTag(*getter_AddRefs(tag));
            if (tag.get() == nsXULAtoms::templateAtom)
                continue;

            // If the element is in the template map, then we
            // assume it's been generated and nuke it.
            nsCOMPtr<nsIContent> tmpl;
            mTemplateMap.GetTemplateFor(child, getter_AddRefs(tmpl));

            if (! tmpl) {
                // No 'template' attribute, so this must not have been
                // generated. We'll need to examine its kids.
                ungenerated.AppendElement(child);
                continue;
            }

            // If we get here, it's "generated". Bye bye!
            element->RemoveChildAt(i, PR_TRUE);
            child->SetDocument(nsnull, PR_TRUE, PR_TRUE);

            // Remove element from the conflict set.
            // XXXwaterson should this be moved into NoteGeneratedSubtreeRemoved?
            nsTemplateMatchSet firings(mConflictSet.GetPool());
            nsTemplateMatchSet retractions(mConflictSet.GetPool());
            mConflictSet.Remove(nsContentTestNode::Element(child), firings, retractions);

            // Remove this and any children from the content support map.
            mContentSupportMap.Remove(child);

            // Remove from the template map
            mTemplateMap.Remove(child);
        }
    }

    return NS_OK;
}

PRBool
nsXULContentBuilder::IsLazyWidgetItem(nsIContent* aElement)
{
    // Determine if this is a <tree>, <treeitem>, or <menu> element
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return PR_FALSE;

    // XXXhyatt Use the XBL service to obtain a base tag.

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_FALSE;

    if ((tag.get() == nsXULAtoms::tree) || (tag.get() == nsXULAtoms::treeitem) ||
        (tag.get() == nsXULAtoms::menu) || (tag.get() == nsXULAtoms::menulist) ||
        (tag.get() == nsXULAtoms::menubutton))
        return PR_TRUE;

    return PR_FALSE;

}

nsresult
nsXULContentBuilder::GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    const char *uri;
    aResource->GetValueConst(&uri);

    nsCOMPtr<nsIDocument> doc;
    mRoot->GetDocument(*getter_AddRefs(doc));
    NS_ASSERTION(doc != nsnull, "root element not in document");
    if (! doc)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
    NS_ASSERTION(xuldoc != nsnull, "expected a XUL document");
    if (! xuldoc)
        return NS_ERROR_FAILURE;

    return xuldoc->GetElementsForID(NS_ConvertUTF8toUCS2(uri), aElements);
}

nsresult
nsXULContentBuilder::CreateElement(PRInt32 aNameSpaceID,
                                    nsIAtom* aTag,
                                    nsIContent** aResult)
{
    nsCOMPtr<nsIDocument> doc;
    mRoot->GetDocument(*getter_AddRefs(doc));
    NS_ASSERTION(doc != nsnull, "not initialized");
    if (! doc)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
    doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
    NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfoManager->GetNodeInfo(aTag, nsnull, aNameSpaceID,
                                 *getter_AddRefs(nodeInfo));

    if (aNameSpaceID == kNameSpaceID_XUL) {
        rv = nsXULElement::Create(nodeInfo, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else if (aNameSpaceID == kNameSpaceID_HTML) {
        rv = gHTMLElementFactory->CreateInstanceByTag(nodeInfo,
                                                      getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(aNameSpaceID, getter_AddRefs(elementFactory));
        rv = elementFactory->CreateInstanceByTag(nodeInfo,
                                                 getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }

    rv = result->SetDocument(doc, PR_FALSE, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's document");
    if (NS_FAILED(rv)) return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULContentBuilder::SetContainerAttrs(nsIContent *aElement, const nsTemplateMatch* aMatch)
{
    NS_PRECONDITION(aMatch->mRule != nsnull, "null ptr");
    if (! aMatch->mRule)
        return NS_ERROR_NULL_POINTER;

    Value containerval;
    aMatch->mAssignments.GetAssignmentFor(aMatch->mRule->GetContainerVariable(), &containerval);

    nsAutoString oldcontainer;
    aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::container, oldcontainer);

    PRBool iscontainer, isempty;
    CheckContainer(VALUE_TO_IRDFRESOURCE(containerval), &iscontainer, &isempty);

    NS_NAMED_LITERAL_STRING(true_, "true");
    NS_NAMED_LITERAL_STRING(false_, "false");
    
    const nsAReadableString& newcontainer =
        iscontainer ? true_ : false_;

    if (oldcontainer != newcontainer) {
        aElement->SetAttr(kNameSpaceID_None, nsXULAtoms::container,
                          newcontainer, PR_TRUE);
    }

    if (! (mFlags & eDontTestEmpty)) {
        nsAutoString oldempty;
        aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::empty, oldempty);

        const nsAReadableString& newempty =
            (iscontainer && isempty) ? true_ : false_;

        if (oldempty != newempty) {
            aElement->SetAttr(kNameSpaceID_None, nsXULAtoms::empty,
                              newempty, PR_TRUE);
        }
    }

    return NS_OK;
}


void 
nsXULContentBuilder::GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult)
{
    gNameSpaceManager->GetElementFactory(aNameSpaceID, aResult);

    if (!*aResult) {
        *aResult = gXMLElementFactory; // Nothing found. Use generic XML element.
        NS_IF_ADDREF(*aResult);
    }
}

//----------------------------------------------------------------------
//
// nsIXULTemplateBuilder methods
//

nsresult
nsXULContentBuilder::Rebuild(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (! IsElementInWidget(aElement))
        return NS_OK;

    // Forbid re-entrant rebuilds
    if (mIsBuilding)
        return NS_OK;

    AutoLatch latch(&mIsBuilding);

    nsresult rv;

    // Next, see if it's a XUL element whose contents have never even
    // been generated. If so, short-circuit and bail; there's nothing
    // for us to "rebuild" yet. They'll get built correctly the next
    // time somebody asks for them. 
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);

    if (xulcontent) {
        PRBool containerContentsBuilt = PR_FALSE;
        xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, containerContentsBuilt);

        if (! containerContentsBuilt)
            return NS_OK;
    }

    // If we get here, then we've tried to generate content for this
    // element. Remove it.
    rv = RemoveGeneratedContent(aElement);
    if (NS_FAILED(rv)) return rv;

    if (aElement == mRoot) {
        // Nuke the content support map and conflict set completely.
        mContentSupportMap.Clear();
        mTemplateMap.Clear();
        mConflictSet.Clear();

        rv = CompileRules();
        if (NS_FAILED(rv)) return rv;
    }

    // Forces the XUL element to remember that it needs to
    // re-generate its children next time around.
    if (xulcontent) {
        xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        xulcontent->ClearLazyState(nsIXULContent::eTemplateContentsBuilt);
        xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);
    }

    // Now, regenerate both the template- and container-generated
    // contents for the current element...
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    CreateTemplateAndContainerContents(aElement, getter_AddRefs(container), &newIndex);

    if (container) {
        nsCOMPtr<nsIDocument> doc;
        mRoot->GetDocument(*getter_AddRefs(doc));
        NS_ASSERTION(doc != nsnull, "root element has no document");
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        doc->ContentAppended(container, newIndex);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULContentBuilder::Rebuild()
{
    return Rebuild(mRoot);
}

//----------------------------------------------------------------------
//
// nsIRDFContentModelBuilder methods
//

NS_IMETHODIMP
nsXULContentBuilder::SetRootContent(nsIContent* aElement)
{
    NS_PRECONDITION(aElement, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    mRoot = aElement;

    return LoadDataSources();
}


NS_IMETHODIMP
nsXULContentBuilder::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    NS_ASSERTION(IsElementInWidget(aElement), "element not managed by this template builder");

    return CreateTemplateAndContainerContents(aElement, nsnull /* don't care */, nsnull /* don't care */);
}

//----------------------------------------------------------------------
//
// nsIDocumentObserver methods
//

NS_IMETHODIMP
nsXULContentBuilder::AttributeChanged(nsIDocument* aDocument,
                                      nsIContent*  aContent,
                                      PRInt32      aNameSpaceID,
                                      nsIAtom*     aAttribute,
                                      PRInt32      aModType, 
                                      PRInt32      aHint)
{
    // Handle "open" and "close" cases. We do this handling before
    // we've notified the observer, so that content is already created
    // for the frame system to walk.
    PRInt32 nameSpaceID = kNameSpaceID_Unknown;
    aContent->GetNameSpaceID(nameSpaceID);

    if ((nameSpaceID == kNameSpaceID_XUL) && (aAttribute == nsXULAtoms::open)) {
        // We're on a XUL tag, and an ``open'' attribute changed.
        nsAutoString open;
        aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);

        if (open == NS_LITERAL_STRING("true"))
            OpenContainer(aContent);
        else
            CloseContainer(aContent);
    }

    // Pass along to the generic template builder.
    return nsXULTemplateBuilder::AttributeChanged(aDocument, aContent, aNameSpaceID, aAttribute, aModType, aHint);
}

NS_IMETHODIMP
nsXULContentBuilder::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
    // Break circular references
    mContentSupportMap.Clear();

    return nsXULTemplateBuilder::DocumentWillBeDestroyed(aDocument);
}


//----------------------------------------------------------------------
//
// nsXULTemplateBuilder methods
//

nsresult
nsXULContentBuilder::ReplaceMatch(nsIRDFResource* aMember,
                                  const nsTemplateMatch* aOldMatch,
                                  nsTemplateMatch* aNewMatch)
{
    if (aOldMatch) {
        // See if we need to yank anything out of the content
        // model to handle the newly matched rule. If the
        // instantiation has a assignment for the content
        // variable, there's content that's been built that we
        // need to pull.
        Value value;
        PRBool hasassignment =
            aOldMatch->mAssignments.GetAssignmentFor(mContentVar, &value);

        NS_ASSERTION(hasassignment, "no content assignment");
        if (! hasassignment)
            return NS_ERROR_UNEXPECTED;

        nsIContent* content = VALUE_TO_ICONTENT(value);

        PRInt32 membervar = aOldMatch->mRule->GetMemberVariable();

        hasassignment = aOldMatch->mAssignments.GetAssignmentFor(membervar, &value);
        NS_ASSERTION(hasassignment, "no member assignment");
        if (! hasassignment)
            return NS_ERROR_UNEXPECTED;

        nsIRDFResource* member = VALUE_TO_IRDFRESOURCE(value);

        RemoveMember(content, member, PR_TRUE);

        if (aNewMatch) {
            // If there's no new match, then go ahead an update the
            // container attributes now.
            SetContainerAttrs(content, aOldMatch);
        }
    }

    if (aNewMatch) {
        // Get the content node to which we were bound
        Value value;
        PRBool hasassignment =
            aNewMatch->mAssignments.GetAssignmentFor(mContentVar, &value);

        NS_ASSERTION(hasassignment, "no content assignment");
        if (! hasassignment)
            return NS_ERROR_UNEXPECTED;

        nsIContent* content = VALUE_TO_ICONTENT(value);

        // Update the 'empty' attribute. Do this *first*, because
        // we may decide to nuke aNewMatch in a minute...
        SetContainerAttrs(content, aNewMatch);

        // See if we've built the container contents for "content"
        // yet. If not, we don't need to build any content. This
        // happens, for example, if we recieve an assertion on a
        // closed folder in a tree widget or on a menu that hasn't
        // yet been dropped.
        PRBool contentsGenerated = PR_TRUE;
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(content);
        if (xulcontent)
            xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);

        if (contentsGenerated) {
            nsCOMPtr<nsIContent> tmpl;
            aNewMatch->mRule->GetContent(getter_AddRefs(tmpl));

            BuildContentFromTemplate(tmpl, content, content, PR_TRUE,
                                     aMember, PR_TRUE, aNewMatch,
                                     nsnull, nsnull);
        }
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::SynchronizeMatch(nsTemplateMatch* match, const VariableSet& modified)
{
    const nsTemplateRule* rule = match->mRule;

    Value memberValue;
    match->mAssignments.GetAssignmentFor(rule->GetMemberVariable(), &memberValue);

    nsIRDFResource* resource = VALUE_TO_IRDFRESOURCE(memberValue);
    NS_ASSERTION(resource != nsnull, "no content");
    if (! resource)
        return NS_ERROR_FAILURE;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* uri;
        resource->GetValueConst(&uri);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] synchronize-all [%s] begin", this, uri));
    }
#endif

    Value parentValue;
    match->mAssignments.GetAssignmentFor(mContentVar, &parentValue);

    nsIContent* parent = VALUE_TO_ICONTENT(parentValue);

    // Now that we've got the resource of the member variable, we
    // should be able to update its kids appropriately
    nsresult rv;
    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(resource, elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG) && cnt == 0) {
        const char* uri;
        resource->GetValueConst(&uri);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] synchronize-all [%s] is not in element map", this, uri));
    }
#endif

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        // If the element is contained by the parent of the rule
        // that we've just matched, then it's the wrong element.
        if (! IsElementContainedBy(element, parent))
            continue;

        nsCOMPtr<nsIContent> templateNode;
        mTemplateMap.GetTemplateFor(element, getter_AddRefs(templateNode));

        if (! templateNode)
            return NS_ERROR_UNEXPECTED;

        // this node was created by a XUL template, so update it accordingly
        rv = SynchronizeUsingTemplate(templateNode, element, *match, modified);
        if (NS_FAILED(rv)) return rv;
    }
        
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* uri;
        resource->GetValueConst(&uri);

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] synchronize-all [%s] end", this, uri));
    }
#endif

    return NS_OK;
}

//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULContentBuilder::OpenContainer(nsIContent* aElement)
{
    nsresult rv;

    // First, make sure that the element is in the right widget -- ours.
    if (! IsElementInWidget(aElement))
        return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    nsXULContentUtils::GetElementRefResource(aElement, getter_AddRefs(resource));

    // If it has no resource, there's nothing that we need to be
    // concerned about here.
    if (! resource)
        return NS_OK;

    // The element has a resource; that means that it corresponds
    // to something in the graph, so we need to go to the graph to
    // create its contents.
    //
    // Create the container's contents "quietly" (i.e., |aNotify ==
    // PR_FALSE|), and then use the |container| and |newIndex| to
    // notify layout where content got created.
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    CreateContainerContents(aElement, resource, PR_FALSE, getter_AddRefs(container), &newIndex);

    if (container && IsLazyWidgetItem(aElement)) {
        // The tree widget is special, and has to be spanked every
        // time we add content to a container.
        nsCOMPtr<nsIDocument> doc;
        mRoot->GetDocument(*getter_AddRefs(doc));
        NS_ASSERTION(doc != nsnull, "root element has no document");
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        rv = doc->ContentAppended(container, newIndex);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::CloseContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (! IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    if (tag.get() == nsXULAtoms::treeitem) {
        // Find the tag that contains the children so that we can
        // remove all of the children. This is a -total- hack, that is
        // necessary for the tree control because...I'm not sure. But
        // it's necessary. Maybe we should fix the tree control to
        // reflow itself when the 'open' attribute changes on a
        // treeitem.
        //
        // OTOH, we could treat this as a (premature?) optimization so
        // that nodes which are not being displayed don't hang around
        // taking up space. Unfortunately, the tree widget currently
        // _relies_ on this behavior and will break if we don't do it
        // :-(.

        // Find the <treechildren> beneath the <treeitem>...
        nsCOMPtr<nsIContent> insertionpoint;
        nsXULContentUtils::FindChildByTag(aElement, kNameSpaceID_XUL,
                                          nsXULAtoms::treechildren,
                                          getter_AddRefs(insertionpoint));

        if (insertionpoint) {
            // ...and blow away all the generated content.
            RemoveGeneratedContent(insertionpoint);
        }

        // Force the XUL element to remember that it needs to re-generate
        // its kids next time around.
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        NS_ASSERTION(xulcontent != nsnull, "not an nsIXULContent");
        if (! xulcontent)
            return NS_ERROR_UNEXPECTED;

        xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);

        // Clear the contents-generated attribute so that the next time we
        // come back, we'll regenerate the kids we just killed.
        xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);

        // Remove any instantiations involving this element from the
        // conflict set.
        nsTemplateMatchSet firings(mConflictSet.GetPool());
        nsTemplateMatchSet retractions(mConflictSet.GetPool());
        mConflictSet.Remove(nsContentTestNode::Element(aElement), firings, retractions);
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::InitializeRuleNetwork()
{
    nsresult rv;
    rv = nsXULTemplateBuilder::InitializeRuleNetwork();
    if (NS_FAILED(rv)) return rv;

    mContentVar = mRules.CreateAnonymousVariable();
    return NS_OK;
}

nsresult
nsXULContentBuilder::InitializeRuleNetworkForSimpleRules(InnerNode** aChildNode)
{
    // For simple rules, the rule network will start off looking
    // something like this:
    //
    //   (root)-->(content ^id ?a)-->(?a ^member ?b)
    //
    nsCOMPtr<nsIDocument> doc;
    mRoot->GetDocument(*getter_AddRefs(doc));
    NS_ASSERTION(doc != nsnull, "root element has no document");
    if (! doc)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
    NS_ASSERTION(xuldoc != nsnull, "expected a XUL Document");
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    nsContentTestNode* idnode =
        new nsContentTestNode(mRules.GetRoot(),
                              mConflictSet,
                              xuldoc,
                              mRoot,
                              mContentVar,
                              mContainerVar,
                              nsnull);

    if (! idnode)
        return NS_ERROR_OUT_OF_MEMORY;

    mRules.GetRoot()->AddChild(idnode);
    mRules.AddNode(idnode);

    // Create (?container ^member ?member)
    nsRDFConMemberTestNode* membernode =
        new nsRDFConMemberTestNode(idnode,
                                   mConflictSet,
                                   mDB,
                                   mContainmentProperties,
                                   mContainerVar,
                                   mMemberVar);

    if (! membernode)
        return NS_ERROR_OUT_OF_MEMORY;

    idnode->AddChild(membernode);
    mRules.AddNode(membernode);

    mRDFTests.Add(membernode);

    *aChildNode = membernode;
    return NS_OK;
}

nsresult
nsXULContentBuilder::CompileCondition(nsIAtom* aTag,
                                      nsTemplateRule* aRule,
                                      nsIContent* aCondition,
                                      InnerNode* aParentNode,
                                      TestNode** aResult)
{
    nsresult rv;

    if (aTag == nsXULAtoms::content) {
        rv = CompileContentCondition(aRule, aCondition, aParentNode, aResult);
    }
    else {
        rv = nsXULTemplateBuilder::CompileCondition(aTag, aRule, aCondition, aParentNode, aResult);
    }

    return rv;
}

nsresult
nsXULContentBuilder::CompileContentCondition(nsTemplateRule* aRule,
                                             nsIContent* aCondition,
                                             InnerNode* aParentNode,
                                             TestNode** aResult)
{
    // Compile a <content> condition, which currently must be of the form:
    //
    //  <content uri="?var" tag="?tag" />
    //
    // XXXwaterson Right now, exactly one <content> condition is
    // required per rule. It creates a nsContentTestNode, binding the
    // content variable to the global content variable that's used
    // during match propogation. The 'uri' attribute must be set.

    // uri
    nsAutoString uri;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::uri, uri);

    if (uri[0] != PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <content> test, expected 'uri' attribute to name a variable", this));

        return NS_OK;
    }

    PRInt32 urivar = mRules.LookupSymbol(uri.get());
    if (! urivar) {
        if (! mContainerSymbol.Length()) {
            // If the container symbol was not explictly declared on
            // the <template> tag, or we haven't seen a previous rule
            // whose <content> condition defined it, then we'll
            // implictly define it *now*.
            mContainerSymbol = uri;
            urivar = mContainerVar;
        }
        else
            urivar = mRules.CreateAnonymousVariable();

        mRules.PutSymbol(uri.get(), urivar);
    }

    // tag
    nsCOMPtr<nsIAtom> tag;

    nsAutoString tagstr;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::tag, tagstr);

    if (tagstr.Length()) {
        tag = dont_AddRef(NS_NewAtom(tagstr));
    }

    nsCOMPtr<nsIDocument> doc;
    mRoot->GetDocument(*getter_AddRefs(doc));
    NS_ASSERTION(doc != nsnull, "root element has no document");
    if (! doc)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
    if (! xuldoc)
        return NS_ERROR_FAILURE;

    // XXXwaterson By binding the content to the global mContentVar,
    // we're essentially saying that each rule *must* have exactly one
    // <content id="?x"/> condition.
    TestNode* testnode = 
        new nsContentTestNode(aParentNode,
                              mConflictSet,
                              xuldoc,
                              mRoot,
                              mContentVar, // XXX see above
                              urivar,
                              tag);

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = testnode;
    return NS_OK;
}

PRBool
nsXULContentBuilder::CompileSimpleAttributeCondition(PRInt32 aNameSpaceID,
                                                     nsIAtom* aAttribute,
                                                     const nsAReadableString& aValue,
                                                     InnerNode* aParentNode,
                                                     TestNode** aResult)
{
    if ((aNameSpaceID == kNameSpaceID_None) && (aAttribute == nsXULAtoms::parent)) {
        // The "parent" test.
        //
        // XXXwaterson this is wrong: we can't add this below the
        // the previous node, because it'll cause an unconstrained
        // search if we ever came "up" through this path. Need a
        // JoinNode in here somewhere.
        nsCOMPtr<nsIAtom> tag = dont_AddRef(NS_NewAtom(aValue));

        *aResult = new nsContentTagTestNode(aParentNode, mConflictSet, mContentVar, tag);
        if (*aResult)
            return PR_TRUE;
    }

    return PR_FALSE;
}
