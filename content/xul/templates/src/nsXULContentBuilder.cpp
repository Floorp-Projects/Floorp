/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsContentCID.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULDocument.h"
#include "nsINodeInfo.h"
#include "nsIServiceManager.h"
#include "nsIXULDocument.h"

#include "nsContentSupportMap.h"
#include "nsRDFConMemberTestNode.h"
#include "nsRDFPropertyTestNode.h"
#include "nsXULSortService.h"
#include "nsTemplateRule.h"
#include "nsTemplateMap.h"
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULElement.h"
#include "nsXULTemplateBuilder.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsAttrName.h"
#include "nsNodeUtils.h"
#include "mozAutoDocUpdate.h"
#include "nsTextNode.h"

#include "jsapi.h"
#include "pldhash.h"
#include "rdf.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
//
// Return values for EnsureElementHasGenericChild()
//
#define NS_ELEMENT_GOT_CREATED NS_RDF_NO_VALUE
#define NS_ELEMENT_WAS_THERE   NS_OK

//----------------------------------------------------------------------
//
// nsXULContentBuilder
//

/**
 * The content builder generates DOM nodes from a template. The actual content
 * generation is done entirely inside BuildContentFromTemplate.
 *
 * Content generation is centered around the generation node (the node with
 * uri="?member" on it). Nodes above the generation node are unique and
 * generated only once. BuildContentFromTemplate will be passed the unique
 * flag as an argument for content at this point and will recurse until it
 * finds the generation node.
 *
 * Once the generation node has been found, the results for that content node
 * are added to the content map, stored in mContentSupportMap.
 *
 * If recursion is allowed, generation continues, where the generation node
 * becomes the container to insert into.
 */
class nsXULContentBuilder : public nsXULTemplateBuilder
{
public:
    // nsIXULTemplateBuilder interface
    NS_IMETHOD CreateContents(nsIContent* aElement, bool aForceCreation);

    NS_IMETHOD HasGeneratedContent(nsIRDFResource* aResource,
                                   nsIAtom* aTag,
                                   bool* aGenerated);

    NS_IMETHOD GetResultForContent(nsIDOMElement* aContent,
                                   nsIXULTemplateResult** aResult);

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

protected:
    friend nsresult
    NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULContentBuilder();

    void Traverse(nsCycleCollectionTraversalCallback &cb) const
    {
        mSortState.Traverse(cb);
    }

    virtual void Uninit(bool aIsFinal);

    // Implementation methods
    nsresult
    OpenContainer(nsIContent* aElement);

    nsresult
    CloseContainer(nsIContent* aElement);

    /**
     * Build content from a template for a given result. This will be called
     * recursively or on demand and will be called for every node in the
     * generated content tree.
     */
    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aResourceNode,
                             nsIContent *aRealNode,
                             bool aIsUnique,
                             bool aIsSelfReference,
                             nsIXULTemplateResult* aChild,
                             bool aNotify,
                             nsTemplateMatch* aMatch,
                             nsIContent** aContainer,
                             int32_t* aNewIndexInContainer);

    /**
     * Copy the attributes from the template node to the node generated
     * from it, performing any substitutions.
     *
     * @param aTemplateNode node within template
     * @param aRealNode generated node to set attibutes upon
     * @param aResult result to look up variable->value bindings in
     * @param aNotify true to notify of DOM changes
     */
    nsresult
    CopyAttributesToElement(nsIContent* aTemplateNode,
                            nsIContent* aRealNode,
                            nsIXULTemplateResult* aResult,
                            bool aNotify);

    /**
     * Add any necessary persistent attributes (persist="...") from the
     * local store to a generated node.
     *
     * @param aTemplateNode node within template
     * @param aRealNode generated node to set persisted attibutes upon
     * @param aResult result to look up variable->value bindings in
     */
    nsresult
    AddPersistentAttributes(Element* aTemplateNode,
                            nsIXULTemplateResult* aResult,
                            nsIContent* aRealNode);

    /**
     * Recalculate any attributes that have variable references. This will
     * be called when a binding has been changed to update the attributes.
     * The attributes are copied from the node aTemplateNode in the template
     * to the generated node aRealNode, using the values from the result
     * aResult. This method will operate recursively.
     *
     * @param aTemplateNode node within template
     * @param aRealNode generated node to set attibutes upon
     * @param aResult result to look up variable->value bindings in
     */
    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             nsIXULTemplateResult* aResult);

    /**
     * Remove the generated node aContent from the DOM and the hashtables
     * used by the content builder.
     */
    nsresult
    RemoveMember(nsIContent* aContent);

    /**
     * Create the appropriate generated content for aElement, by calling
     * CreateContainerContents.
     *
     * @param aElement element to generate content inside
     * @param aForceCreation true to force creation for closed items such as menus
     */
    nsresult
    CreateTemplateAndContainerContents(nsIContent* aElement,
                                       bool aForceCreation);

    /**
     * Generate the results for a template, by calling
     * CreateContainerContentsForQuerySet for each queryset.
     *
     * @param aElement element to generate content inside
     * @param aResult reference point for query
     * @param aForceCreation true to force creation for closed items such as menus
     * @param aNotify true to notify of DOM changes as each element is inserted
     * @param aNotifyAtEnd notify at the end of all DOM changes
     */
    nsresult
    CreateContainerContents(nsIContent* aElement,
                            nsIXULTemplateResult* aResult,
                            bool aForceCreation,
                            bool aNotify,
                            bool aNotifyAtEnd);

    /**
     * Generate the results for a query.
     *
     * @param aElement element to generate content inside
     * @param aResult reference point for query
     * @param aNotify true to notify of DOM changes
     * @param aContainer container content was added inside
     * @param aNewIndexInContainer index with container in which content was added
     */
    nsresult
    CreateContainerContentsForQuerySet(nsIContent* aElement,
                                       nsIXULTemplateResult* aResult,
                                       bool aNotify,
                                       nsTemplateQuerySet* aQuerySet,
                                       nsIContent** aContainer,
                                       int32_t* aNewIndexInContainer);

    /**
     * Check if an element with a particular tag exists with a container.
     * If it is not present, append a new element with that tag into the
     * container.
     *
     * @param aParent parent container
     * @param aNameSpaceID namespace of tag to locate or create
     * @param aTag tag to locate or create
     * @param aNotify true to notify of DOM changes
     * @param aResult set to the found or created node.
     */
    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aTag,
                                 bool aNotify,
                                 nsIContent** aResult);

    bool
    IsOpen(nsIContent* aElement);

    nsresult
    RemoveGeneratedContent(nsIContent* aElement);

    nsresult
    GetElementsForResult(nsIXULTemplateResult* aResult,
                         nsCOMArray<nsIContent>& aElements);

    nsresult
    CreateElement(int32_t aNameSpaceID,
                  nsIAtom* aTag,
                  nsIContent** aResult);

    /**
     * Set the container and empty attributes on a node. If
     * aIgnoreNonContainers is true, then the element is not changed
     * for non-containers. Otherwise, the container attribute will be set to
     * false.
     *
     * @param aElement element to set attributes on
     * @param aResult result to use to determine state of attributes
     * @param aIgnoreNonContainers true to not change for non-containers
     * @param aNotify true to notify of DOM changes
     */
    nsresult
    SetContainerAttrs(nsIContent *aElement,
                      nsIXULTemplateResult* aResult,
                      bool aIgnoreNonContainers,
                      bool aNotify);

    virtual nsresult
    RebuildAll();

    // GetInsertionLocations, ReplaceMatch and SynchronizeResult are inherited
    // from nsXULTemplateBuilder

    /**
     * Return true if the result can be inserted into the template as
     * generated content. For the content builder, aLocations will be set
     * to the list of containers where the content should be inserted.
     */
    virtual bool
    GetInsertionLocations(nsIXULTemplateResult* aOldResult,
                          nsCOMArray<nsIContent>** aLocations);

    /**
     * Remove the content associated with aOldResult which no longer matches,
     * and/or generate content for a new match.
     */
    virtual nsresult
    ReplaceMatch(nsIXULTemplateResult* aOldResult,
                 nsTemplateMatch* aNewMatch,
                 nsTemplateRule* aNewMatchRule,
                 void *aContext);

    /**
     * Synchronize a result bindings with the generated content for that
     * result. This will be called as a result of the template builder's
     * ResultBindingChanged method.
     */
    virtual nsresult
    SynchronizeResult(nsIXULTemplateResult* aResult);

    /**
     * Compare a result to a content node. If the generated content for the
     * result should come before aContent, set aSortOrder to -1. If it should
     * come after, set sortOrder to 1. If both are equal, set to 0.
     */
    nsresult
    CompareResultToNode(nsIXULTemplateResult* aResult, nsIContent* aContent,
                        int32_t* aSortOrder);

    /**
     * Insert a generated node into the container where it should go according
     * to the current sort. aNode is the generated content node and aResult is
     * the result for the generated node.
     */
    nsresult
    InsertSortedNode(nsIContent* aContainer,
                     nsIContent* aNode,
                     nsIXULTemplateResult* aResult,
                     bool aNotify);

    /**
     * Maintains a mapping between elements in the DOM and the matches
     * that they support.
     */
    nsContentSupportMap mContentSupportMap;

    /**
     * Maintains a mapping from an element in the DOM to the template
     * element that it was created from.
     */
    nsTemplateMap mTemplateMap;

    /**
     * Information about the currently active sort
     */
    nsSortState mSortState;
};

nsresult
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nullptr, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsresult rv;
    nsXULContentBuilder* result = new nsXULContentBuilder();
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->InitGlobals();

    if (NS_SUCCEEDED(rv))
        rv = result->QueryInterface(aIID, aResult);

    NS_RELEASE(result);
    return rv;
}

nsXULContentBuilder::nsXULContentBuilder()
{
  mSortState.initialized = false;
}

void
nsXULContentBuilder::Uninit(bool aIsFinal)
{
    if (! aIsFinal && mRoot) {
        nsresult rv = RemoveGeneratedContent(mRoot);
        if (NS_FAILED(rv))
            return;
    }

    // Nuke the content support map completely.
    mContentSupportMap.Clear();
    mTemplateMap.Clear();

    mSortState.initialized = false;

    nsXULTemplateBuilder::Uninit(aIsFinal);
}

nsresult
nsXULContentBuilder::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                              nsIContent *aResourceNode,
                                              nsIContent *aRealNode,
                                              bool aIsUnique,
                                              bool aIsSelfReference,
                                              nsIXULTemplateResult* aChild,
                                              bool aNotify,
                                              nsTemplateMatch* aMatch,
                                              nsIContent** aContainer,
                                              int32_t* aNewIndexInContainer)
{
    // This is the mother lode. Here is where we grovel through an
    // element in the template, copying children from the template
    // into the "real" content tree, performing substitution as we go
    // by looking stuff up using the results.
    //
    //   |aTemplateNode| is the element in the "template tree", whose
    //   children we will duplicate and move into the "real" content
    //   tree.
    //
    //   |aResourceNode| is the element in the "real" content tree that
    //   has the "id" attribute set to an result's id. This is
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
    //   |aIsSelfReference| should be set to "true" for cases where
    //   the reference and member variables are the same, indicating
    //   that the generated node is the same as the reference point,
    //   so generation should not recurse, or else an infinite loop
    //   would occur.
    //
    //   |aChild| is the result for which we are building content.
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
    // notification to be propagated to document observers.
    //

    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("nsXULContentBuilder::BuildContentFromTemplate (is unique: %d)",
               aIsUnique));

        nsAutoString id;
        aChild->GetId(id);

        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("Tags: [Template: %s  Resource: %s  Real: %s] for id %s",
                nsAtomCString(aTemplateNode->Tag()).get(), 
                nsAtomCString(aResourceNode->Tag()).get(),
                nsAtomCString(aRealNode->Tag()).get(), NS_ConvertUTF16toUTF8(id).get()));
    }
#endif

    // Iterate through all of the template children, constructing
    // "real" content model nodes for each "template" child.
    for (nsIContent* tmplKid = aTemplateNode->GetFirstChild();
         tmplKid;
         tmplKid = tmplKid->GetNextSibling()) {

        int32_t nameSpaceID = tmplKid->GetNameSpaceID();

        // Check whether this element is the generation element. The generation
        // element is the element that is cookie-cutter copied once for each
        // different result specified by |aChild|.
        //
        // Nodes that appear -above- the generation element
        // (that is, are ancestors of the generation element in the
        // content model) are unique across all values of |aChild|,
        // and are created only once.
        //
        // Nodes that appear -below- the generation element (that is,
        // are descendants of the generation element in the content
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
        // The <treeitem> element [2] is the generation element. This
        // element, and all of its descendants ([3], [4], and [5])
        // will be duplicated for each different |aChild|.
        // It's ancestor <treechildren> [1] is unique, and
        // will only be created -once-, no matter how many <treeitem>s
        // are created below it.
        //
        // isUnique will be true for nodes above the generation element,
        // isGenerationElement will be true for the generation element,
        // and both will be false for descendants
        bool isGenerationElement = false;
        bool isUnique = aIsUnique;

        // We identify the resource element by presence of a
        // "uri='rdf:*'" attribute. (We also support the older
        // "uri='...'" syntax.)
        if (tmplKid->HasAttr(kNameSpaceID_None, nsGkAtoms::uri) && aMatch->IsActive()) {
            isGenerationElement = true;
            isUnique = false;
        }

        MOZ_ASSERT_IF(isGenerationElement, tmplKid->IsElement());

        nsIAtom *tag = tmplKid->Tag();

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p]     building %s %s %s",
                    this, nsAtomCString(tag).get(),
                    (isGenerationElement ? "[resource]" : ""),
                    (isUnique ? "[unique]" : "")));
        }
#endif

        // Set to true if the child we're trying to create now
        // already existed in the content model.
        bool realKidAlreadyExisted = false;

        nsCOMPtr<nsIContent> realKid;
        if (isUnique) {
            // The content is "unique"; that is, we haven't descended
            // far enough into the template to hit the generation
            // element yet. |EnsureElementHasGenericChild()| will
            // conditionally create the element iff it isn't there
            // already.
            rv = EnsureElementHasGenericChild(aRealNode, nameSpaceID, tag, aNotify, getter_AddRefs(realKid));
            if (NS_FAILED(rv))
                return rv;

            if (rv == NS_ELEMENT_WAS_THERE) {
                realKidAlreadyExisted = true;
            }
            else {
                // Potentially remember the index of this element as the first
                // element that we've generated. Note that we remember
                // this -before- we recurse!
                if (aContainer && !*aContainer) {
                    *aContainer = aRealNode;
                    NS_ADDREF(*aContainer);

                    uint32_t indx = aRealNode->GetChildCount();

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
            rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, true,
                                          aIsSelfReference, aChild, aNotify, aMatch,
                                          aContainer, aNewIndexInContainer);

            if (NS_FAILED(rv))
                return rv;
        }
        else if (isGenerationElement) {
            // It's the "resource" element. Create a new element using
            // the namespace ID and tag from the template element.
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv))
                return rv;

            // Add the resource element to the content support map so
            // we can remove the match based on the content node later.
            mContentSupportMap.Put(realKid, aMatch);

            // Assign the element an 'id' attribute using result's id
            nsAutoString id;
            rv = aChild->GetId(id);
            if (NS_FAILED(rv))
                return rv;

            rv = realKid->SetAttr(kNameSpaceID_None, nsGkAtoms::id, id, false);
            if (NS_FAILED(rv))
                return rv;

            // Set up the element's 'container' and 'empty' attributes.
            SetContainerAttrs(realKid, aChild, true, false);
        }
        else if (tag == nsGkAtoms::textnode &&
                 nameSpaceID == kNameSpaceID_XUL) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the 'rdf:resource' attribute for the
            // given node.
            // SynchronizeUsingTemplate contains code used to update textnodes,
            // so make sure to modify both when changing this
            PRUnichar attrbuf[128];
            nsFixedString attrValue(attrbuf, ArrayLength(attrbuf), 0);
            tmplKid->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);
            if (!attrValue.IsEmpty()) {
                nsAutoString value;
                rv = SubstituteText(aChild, attrValue, value);
                if (NS_FAILED(rv)) return rv;

                nsRefPtr<nsTextNode> content =
                  new nsTextNode(mRoot->NodeInfo()->NodeInfoManager());

                content->SetText(value, false);

                rv = aRealNode->AppendChildTo(content, aNotify);
                if (NS_FAILED(rv)) return rv;

                // XXX Don't bother remembering text nodes as the
                // first element we've generated?
            }
        }
        else if (tmplKid->IsNodeOfType(nsINode::eTEXT)) {
            nsCOMPtr<nsIDOMNode> tmplTextNode = do_QueryInterface(tmplKid);
            if (!tmplTextNode) {
                NS_ERROR("textnode not implementing nsIDOMNode??");
                return NS_ERROR_FAILURE;
            }
            nsCOMPtr<nsIDOMNode> clonedNode;
            tmplTextNode->CloneNode(false, 1, getter_AddRefs(clonedNode));
            nsCOMPtr<nsIContent> clonedContent = do_QueryInterface(clonedNode);
            if (!clonedContent) {
                NS_ERROR("failed to clone textnode");
                return NS_ERROR_FAILURE;
            }
            rv = aRealNode->AppendChildTo(clonedContent, aNotify);
            if (NS_FAILED(rv)) return rv;
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

                uint32_t indx = aRealNode->GetChildCount();

                // Since we haven't inserted any content yet, our new
                // index in the container will be the current count of
                // elements in the container.
                *aNewIndexInContainer = indx;
            }

            // Remember the template kid from which we created the
            // real kid. This allows us to sync back up with the
            // template to incrementally build content.
            mTemplateMap.Put(realKid, tmplKid);

            rv = CopyAttributesToElement(tmplKid, realKid, aChild, false);
            if (NS_FAILED(rv)) return rv;

            // Add any persistent attributes
            if (isGenerationElement) {
                rv = AddPersistentAttributes(tmplKid->AsElement(), aChild,
                                             realKid);
                if (NS_FAILED(rv)) return rv;
            }

            // the unique content recurses up above. Also, don't recurse if
            // this is a self reference (a reference to the same resource)
            // or we'll end up regenerating the same content.
            if (!aIsSelfReference && !isUnique) {
                // this call creates the content inside the generation node,
                // for example the label below:
                //  <vbox uri="?">
                //    <label value="?title"/>
                //  </vbox>
                rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, false,
                                              false, aChild, false, aMatch,
                                              nullptr /* don't care */,
                                              nullptr /* don't care */);
                if (NS_FAILED(rv)) return rv;

                if (isGenerationElement) {
                    // build the next level of children
                    rv = CreateContainerContents(realKid, aChild, false,
                                                 false, false);
                    if (NS_FAILED(rv)) return rv;
                }
            }

            // We'll _already_ have added the unique elements; but if
            // it's -not- unique, then use the XUL sort service now to
            // append the element to the content model.
            if (! isUnique) {
                rv = NS_ERROR_UNEXPECTED;

                if (isGenerationElement)
                    rv = InsertSortedNode(aRealNode, realKid, aChild, aNotify);

                if (NS_FAILED(rv)) {
                    rv = aRealNode->AppendChildTo(realKid, aNotify);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element");
                }
            }
        }
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::CopyAttributesToElement(nsIContent* aTemplateNode,
                                             nsIContent* aRealNode,
                                             nsIXULTemplateResult* aResult,
                                             bool aNotify)
{
    nsresult rv;

    // Copy all attributes from the template to the new element
    uint32_t numAttribs = aTemplateNode->GetAttrCount();

    for (uint32_t attr = 0; attr < numAttribs; attr++) {
        const nsAttrName* name = aTemplateNode->GetAttrNameAt(attr);
        int32_t attribNameSpaceID = name->NamespaceID();
        // Hold a strong reference here so that the atom doesn't go away
        // during UnsetAttr.
        nsCOMPtr<nsIAtom> attribName = name->LocalName();

        // XXXndeakin ignore namespaces until bug 321182 is fixed
        if (attribName != nsGkAtoms::id && attribName != nsGkAtoms::uri) {
            // Create a buffer here, because there's a chance that an
            // attribute in the template is going to be an RDF URI, which is
            // usually longish.
            PRUnichar attrbuf[128];
            nsFixedString attribValue(attrbuf, ArrayLength(attrbuf), 0);
            aTemplateNode->GetAttr(attribNameSpaceID, attribName, attribValue);
            if (!attribValue.IsEmpty()) {
                nsAutoString value;
                rv = SubstituteText(aResult, attribValue, value);
                if (NS_FAILED(rv))
                    return rv;

                // if the string is empty after substitutions, remove the
                // attribute
                if (!value.IsEmpty()) {
                    rv = aRealNode->SetAttr(attribNameSpaceID,
                                            attribName,
                                            name->GetPrefix(),
                                            value,
                                            aNotify);
                }
                else {
                    rv = aRealNode->UnsetAttr(attribNameSpaceID,
                                              attribName,
                                              aNotify);
                }

                if (NS_FAILED(rv))
                    return rv;
            }
        }
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::AddPersistentAttributes(Element* aTemplateNode,
                                             nsIXULTemplateResult* aResult,
                                             nsIContent* aRealNode)
{
    if (!mRoot)
        return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = GetResultResource(aResult, getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString attribute, persist;
    aTemplateNode->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);

    while (!persist.IsEmpty()) {
        attribute.Truncate();

        int32_t offset = persist.FindCharInSet(" ,");
        if (offset > 0) {
            persist.Left(attribute, offset);
            persist.Cut(0, offset + 1);
        }
        else {
            attribute = persist;
            persist.Truncate();
        }

        attribute.Trim(" ");

        if (attribute.IsEmpty())
            break;

        nsCOMPtr<nsIAtom> tag;
        int32_t nameSpaceID;

        nsCOMPtr<nsINodeInfo> ni =
            aTemplateNode->GetExistingAttrNameFromQName(attribute);
        if (ni) {
            tag = ni->NameAtom();
            nameSpaceID = ni->NamespaceID();
        }
        else {
            tag = do_GetAtom(attribute);
            NS_ENSURE_TRUE(tag, NS_ERROR_OUT_OF_MEMORY);

            nameSpaceID = kNameSpaceID_None;
        }

        nsCOMPtr<nsIRDFResource> property;
        rv = nsXULContentUtils::GetResource(nameSpaceID, tag, getter_AddRefs(property));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIRDFNode> target;
        rv = mDB->GetTarget(resource, property, true, getter_AddRefs(target));
        NS_ENSURE_SUCCESS(rv, rv);

        if (! target)
            continue;

        nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(target);
        NS_ASSERTION(value != nullptr, "unable to stomach that sort of node");
        if (! value)
            continue;

        const PRUnichar* valueStr;
        rv = value->GetValueConst(&valueStr);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aRealNode->SetAttr(nameSpaceID, tag, nsDependentString(valueStr),
                                false);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::SynchronizeUsingTemplate(nsIContent* aTemplateNode,
                                              nsIContent* aRealElement,
                                              nsIXULTemplateResult* aResult)
{
    // check all attributes on the template node; if they reference a resource,
    // update the equivalent attribute on the content node
    nsresult rv;
    rv = CopyAttributesToElement(aTemplateNode, aRealElement, aResult, true);
    if (NS_FAILED(rv))
        return rv;

    uint32_t count = aTemplateNode->GetChildCount();

    for (uint32_t loop = 0; loop < count; ++loop) {
        nsIContent *tmplKid = aTemplateNode->GetChildAt(loop);

        if (! tmplKid)
            break;

        nsIContent *realKid = aRealElement->GetChildAt(loop);
        if (! realKid)
            break;

        // check for text nodes and update them accordingly.
        // This code is similar to that in BuildContentFromTemplate
        if (tmplKid->NodeInfo()->Equals(nsGkAtoms::textnode,
                                        kNameSpaceID_XUL)) {
            PRUnichar attrbuf[128];
            nsFixedString attrValue(attrbuf, ArrayLength(attrbuf), 0);
            tmplKid->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);
            if (!attrValue.IsEmpty()) {
                nsAutoString value;
                rv = SubstituteText(aResult, attrValue, value);
                if (NS_FAILED(rv)) return rv;
                realKid->SetText(value, true);
            }
        }

        rv = SynchronizeUsingTemplate(tmplKid, realKid, aResult);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::RemoveMember(nsIContent* aContent)
{
    nsCOMPtr<nsIContent> parent = aContent->GetParent();
    if (parent) {
        int32_t pos = parent->IndexOf(aContent);

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) return NS_OK;

        // Note: RemoveChildAt sets |child|'s document to null so that
        // it'll get knocked out of the XUL doc's resource-to-element
        // map.
        parent->RemoveChildAt(pos, true);
    }

    // Remove from the content support map.
    mContentSupportMap.Remove(aContent);

    // Remove from the template map
    mTemplateMap.Remove(aContent);

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateTemplateAndContainerContents(nsIContent* aElement,
                                                        bool aForceCreation)
{
    // Generate both 1) the template content for the current element,
    // and 2) recursive subcontent (if the current element refers to a
    // container result).

    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULContentBuilder::CreateTemplateAndContainerContents start - flags: %d",
            mFlags));

    if (! mQueryProcessor)
        return NS_OK;

    // for the root element, get the ref attribute and generate content
    if (aElement == mRoot) {
        if (! mRootResult) {
            nsAutoString ref;
            mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::ref, ref);

            if (! ref.IsEmpty()) {
                nsresult rv = mQueryProcessor->TranslateRef(mDataSource, ref,
                                                            getter_AddRefs(mRootResult));
                if (NS_FAILED(rv))
                    return rv;
            }
        }

        if (mRootResult) {
            CreateContainerContents(aElement, mRootResult, aForceCreation,
                                    false, true);
        }
    }
    else if (!(mFlags & eDontRecurse)) {
        // The content map will contain the generation elements (the ones that
        // are given ids) and only those elements, so get the reference point
        // from the corresponding match.
        nsTemplateMatch *match = nullptr;
        if (mContentSupportMap.Get(aElement, &match))
            CreateContainerContents(aElement, match->mResult, aForceCreation,
                                    false, true);
    }

    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULContentBuilder::CreateTemplateAndContainerContents end"));

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateContainerContents(nsIContent* aElement,
                                             nsIXULTemplateResult* aResult,
                                             bool aForceCreation,
                                             bool aNotify,
                                             bool aNotifyAtEnd)
{
    if (!aForceCreation && !IsOpen(aElement))
        return NS_OK;

    // don't generate children if recursion or child processing isn't allowed
    if (aResult != mRootResult) {
        if (mFlags & eDontRecurse)
            return NS_OK;

        bool mayProcessChildren;
        nsresult rv = aResult->GetMayProcessChildren(&mayProcessChildren);
        if (NS_FAILED(rv) || !mayProcessChildren)
            return rv;
    }

    nsCOMPtr<nsIRDFResource> refResource;
    GetResultResource(aResult, getter_AddRefs(refResource));
    if (! refResource)
        return NS_ERROR_FAILURE;

    // Avoid re-entrant builds for the same resource.
    if (IsActivated(refResource))
        return NS_OK;

    ActivationEntry entry(refResource, &mTop);

    // Compile the rules now, if they haven't been already.
    if (! mQueriesCompiled) {
        nsresult rv = CompileQueries();
        if (NS_FAILED(rv))
            return rv;
    }

    if (mQuerySets.Length() == 0)
        return NS_OK;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsXULElement *xulcontent = nsXULElement::FromContent(aElement);
    if (xulcontent) {
        if (xulcontent->GetTemplateGenerated())
            return NS_OK;

        // Now mark the element's contents as being generated so that
        // any re-entrant calls don't trigger an infinite recursion.
        xulcontent->SetTemplateGenerated();
    }

    int32_t newIndexInContainer = -1;
    nsIContent* container = nullptr;

    int32_t querySetCount = mQuerySets.Length();

    for (int32_t r = 0; r < querySetCount; r++) {
        nsTemplateQuerySet* queryset = mQuerySets[r];

        nsIAtom* tag = queryset->GetTag();
        if (tag && tag != aElement->Tag())
            continue;

        CreateContainerContentsForQuerySet(aElement, aResult, aNotify, queryset,
                                           &container, &newIndexInContainer);
    }

    if (aNotifyAtEnd && container) {
        MOZ_AUTO_DOC_UPDATE(container->GetCurrentDoc(), UPDATE_CONTENT_MODEL,
                            true);
        nsNodeUtils::ContentAppended(container,
                                     container->GetChildAt(newIndexInContainer),
                                     newIndexInContainer);
    }

    NS_IF_RELEASE(container);

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateContainerContentsForQuerySet(nsIContent* aElement,
                                                        nsIXULTemplateResult* aResult,
                                                        bool aNotify,
                                                        nsTemplateQuerySet* aQuerySet,
                                                        nsIContent** aContainer,
                                                        int32_t* aNewIndexInContainer)
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsAutoString id;
        aResult->GetId(id);
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("nsXULContentBuilder::CreateContainerContentsForQuerySet start for ref %s\n",
               NS_ConvertUTF16toUTF8(id).get()));
    }
#endif

    if (! mQueryProcessor)
        return NS_OK;

    nsCOMPtr<nsISimpleEnumerator> results;
    nsresult rv = mQueryProcessor->GenerateResults(mDataSource, aResult,
                                                   aQuerySet->mCompiledQuery,
                                                   getter_AddRefs(results));
    if (NS_FAILED(rv) || !results)
        return rv;

    bool hasMoreResults;
    rv = results->HasMoreElements(&hasMoreResults);

    for (; NS_SUCCEEDED(rv) && hasMoreResults;
           rv = results->HasMoreElements(&hasMoreResults)) {
        nsCOMPtr<nsISupports> nr;
        rv = results->GetNext(getter_AddRefs(nr));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIXULTemplateResult> nextresult = do_QueryInterface(nr);
        if (!nextresult)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIRDFResource> resultid;
        rv = GetResultResource(nextresult, getter_AddRefs(resultid));
        if (NS_FAILED(rv))
            return rv;

        if (!resultid)
            continue;

        nsTemplateMatch *newmatch =
            nsTemplateMatch::Create(aQuerySet->Priority(),
                                    nextresult, aElement);
        if (!newmatch)
            return NS_ERROR_OUT_OF_MEMORY;

        // check if there is already an existing match. If so, a previous
        // query already generated content so the match is just added to the
        // end of the set of matches.

        bool generateContent = true;

        nsTemplateMatch* prevmatch = nullptr;
        nsTemplateMatch* existingmatch = nullptr;
        nsTemplateMatch* removematch = nullptr;
        if (mMatchMap.Get(resultid, &existingmatch)){
            // check if there is an existing match that matched a rule
            while (existingmatch) {
                // break out once we've reached a query in the list with a
                // higher priority, as the new match list is sorted by
                // priority, and the new match should be inserted here
                int32_t priority = existingmatch->QuerySetPriority();
                if (priority > aQuerySet->Priority())
                    break;

                // skip over non-matching containers 
                if (existingmatch->GetContainer() == aElement) {
                    // if the same priority is already found, replace it. This can happen
                    // when a container is removed and readded
                    if (priority == aQuerySet->Priority()) {
                        removematch = existingmatch;
                        break;
                    }

                    if (existingmatch->IsActive())
                        generateContent = false;
                }

                prevmatch = existingmatch;
                existingmatch = existingmatch->mNext;
            }
        }

        if (removematch) {
            // remove the generated content for the existing match
            rv = ReplaceMatch(removematch->mResult, nullptr, nullptr, aElement);
            if (NS_FAILED(rv))
                return rv;

            if (mFlags & eLoggingEnabled)
                OutputMatchToLog(resultid, removematch, false);
        }

        if (generateContent) {
            // find the rule that matches. If none match, the content does not
            // need to be generated

            int16_t ruleindex;
            nsTemplateRule* matchedrule = nullptr;
            rv = DetermineMatchedRule(aElement, nextresult, aQuerySet,
                                      &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(newmatch, false);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule,
                                           ruleindex, nextresult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(newmatch, false);
                    return rv;
                }

                // Grab the template node
                nsCOMPtr<nsIContent> action = matchedrule->GetAction();
                BuildContentFromTemplate(action, aElement, aElement, true,
                                         mRefVariable == matchedrule->GetMemberVariable(),
                                         nextresult, aNotify, newmatch,
                                         aContainer, aNewIndexInContainer);
            }
        }

        if (mFlags & eLoggingEnabled)
            OutputMatchToLog(resultid, newmatch, true);

        if (prevmatch) {
            prevmatch->mNext = newmatch;
        }
        else {
            mMatchMap.Put(resultid, newmatch);
        }

        if (removematch) {
            newmatch->mNext = removematch->mNext;
            nsTemplateMatch::Destroy(removematch, true);
        }
        else {
            newmatch->mNext = existingmatch;
        }
    }

    return rv;
}

nsresult
nsXULContentBuilder::EnsureElementHasGenericChild(nsIContent* parent,
                                                  int32_t nameSpaceID,
                                                  nsIAtom* tag,
                                                  bool aNotify,
                                                  nsIContent** result)
{
    nsresult rv;

    rv = nsXULContentUtils::FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv))
        return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        rv = CreateElement(nameSpaceID, tag, getter_AddRefs(element));
        if (NS_FAILED(rv))
            return rv;

        // XXX Note that the notification ensures we won't batch insertions! This could be bad! - Dave
        rv = parent->AppendChildTo(element, aNotify);
        if (NS_FAILED(rv))
            return rv;

        *result = element;
        NS_ADDREF(*result);
        return NS_ELEMENT_GOT_CREATED;
    }
    else {
        return NS_ELEMENT_WAS_THERE;
    }
}

bool
nsXULContentBuilder::IsOpen(nsIContent* aElement)
{
    // Determine if this is a <treeitem> or <menu> element
    if (!aElement->IsXUL())
        return true;

    // XXXhyatt Use the XBL service to obtain a base tag.
    nsIAtom *tag = aElement->Tag();
    if (tag == nsGkAtoms::menu ||
        tag == nsGkAtoms::menubutton ||
        tag == nsGkAtoms::toolbarbutton ||
        tag == nsGkAtoms::button ||
        tag == nsGkAtoms::treeitem)
        return aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                     nsGkAtoms::_true, eCaseMatters);
    return true;
}

nsresult
nsXULContentBuilder::RemoveGeneratedContent(nsIContent* aElement)
{
    // Keep a queue of "ungenerated" elements that we have to probe
    // for generated content.
    nsAutoTArray<nsIContent*, 8> ungenerated;
    if (ungenerated.AppendElement(aElement) == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;

    uint32_t count;
    while (0 != (count = ungenerated.Length())) {
        // Pull the next "ungenerated" element off the queue.
        uint32_t last = count - 1;
        nsCOMPtr<nsIContent> element = ungenerated[last];
        ungenerated.RemoveElementAt(last);

        uint32_t i = element->GetChildCount();

        while (i-- > 0) {
            nsCOMPtr<nsIContent> child = element->GetChildAt(i);

            // Optimize for the <template> element, because we *know*
            // it won't have any generated content: there's no reason
            // to even check this subtree.
            // XXX should this check |child| rather than |element|? Otherwise
            //     it should be moved outside the inner loop. Bug 297290.
            if (element->NodeInfo()->Equals(nsGkAtoms::_template,
                                            kNameSpaceID_XUL) ||
                !element->IsElement())
                continue;

            // If the element is in the template map, then we
            // assume it's been generated and nuke it.
            nsCOMPtr<nsIContent> tmpl;
            mTemplateMap.GetTemplateFor(child, getter_AddRefs(tmpl));

            if (! tmpl) {
                // No 'template' attribute, so this must not have been
                // generated. We'll need to examine its kids.
                if (ungenerated.AppendElement(child) == nullptr)
                    return NS_ERROR_OUT_OF_MEMORY;
                continue;
            }

            // If we get here, it's "generated". Bye bye!
            element->RemoveChildAt(i, true);

            // Remove this and any children from the content support map.
            mContentSupportMap.Remove(child);

            // Remove from the template map
            mTemplateMap.Remove(child);
        }
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::GetElementsForResult(nsIXULTemplateResult* aResult,
                                          nsCOMArray<nsIContent>& aElements)
{
    // if the root has been removed from the document, just return
    // since there won't be any generated content any more
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mRoot->GetDocument());
    if (! xuldoc)
        return NS_OK;

    nsAutoString id;
    aResult->GetId(id);

    xuldoc->GetElementsForID(id, aElements);

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateElement(int32_t aNameSpaceID,
                                   nsIAtom* aTag,
                                   nsIContent** aResult)
{
    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc != nullptr, "not initialized");
    if (! doc)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIContent> result;
    nsCOMPtr<nsINodeInfo> nodeInfo =
        doc->NodeInfoManager()->GetNodeInfo(aTag, nullptr, aNameSpaceID,
                                            nsIDOMNode::ELEMENT_NODE);

    nsresult rv = NS_NewElement(getter_AddRefs(result), nodeInfo.forget(),
                                NOT_FROM_PARSER);
    if (NS_FAILED(rv))
        return rv;

    result.forget(aResult);
    return NS_OK;
}

nsresult
nsXULContentBuilder::SetContainerAttrs(nsIContent *aElement,
                                       nsIXULTemplateResult* aResult,
                                       bool aIgnoreNonContainers,
                                       bool aNotify)
{
    NS_PRECONDITION(aResult != nullptr, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    bool iscontainer;
    aResult->GetIsContainer(&iscontainer);

    if (aIgnoreNonContainers && !iscontainer)
        return NS_OK;

    NS_NAMED_LITERAL_STRING(true_, "true");
    NS_NAMED_LITERAL_STRING(false_, "false");

    const nsAString& newcontainer =
        iscontainer ? true_ : false_;

    aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::container,
                      newcontainer, aNotify);

    if (iscontainer && !(mFlags & eDontTestEmpty)) {
        bool isempty;
        aResult->GetIsEmpty(&isempty);

        const nsAString& newempty =
            (iscontainer && isempty) ? true_ : false_;

        aElement->SetAttr(kNameSpaceID_None, nsGkAtoms::empty,
                          newempty, aNotify);
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIXULTemplateBuilder methods
//

NS_IMETHODIMP
nsXULContentBuilder::CreateContents(nsIContent* aElement, bool aForceCreation)
{
    NS_PRECONDITION(aElement != nullptr, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // don't build contents for closed elements. aForceCreation will be true
    // when a menu is about to be opened, so the content should be built anyway.
    if (!aForceCreation && !IsOpen(aElement))
        return NS_OK;

    return CreateTemplateAndContainerContents(aElement, aForceCreation);
}

NS_IMETHODIMP
nsXULContentBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                         nsIAtom* aTag,
                                         bool* aGenerated)
{
    *aGenerated = false;
    NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_STATE(mRootResult);

    nsCOMPtr<nsIRDFResource> rootresource;
    nsresult rv = mRootResult->GetResource(getter_AddRefs(rootresource));
    if (NS_FAILED(rv))
        return rv;

    // the root resource is always acceptable
    if (aResource == rootresource) {
        if (!aTag || mRoot->Tag() == aTag)
            *aGenerated = true;
    }
    else {
        const char* uri;
        aResource->GetValueConst(&uri);

        NS_ConvertUTF8toUTF16 refID(uri);

        // just return if the node is no longer in a document
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mRoot->GetDocument());
        if (! xuldoc)
            return NS_OK;

        nsCOMArray<nsIContent> elements;
        xuldoc->GetElementsForID(refID, elements);

        uint32_t cnt = elements.Count();

        for (int32_t i = int32_t(cnt) - 1; i >= 0; --i) {
            nsCOMPtr<nsIContent> content = elements.SafeObjectAt(i);

            do {
                nsTemplateMatch* match;
                if (content == mRoot || mContentSupportMap.Get(content, &match)) {
                    // If we've got a tag, check it to ensure we're consistent.
                    if (!aTag || content->Tag() == aTag) {
                        *aGenerated = true;
                        return NS_OK;
                    }
                }

                content = content->GetParent();
            } while (content);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULContentBuilder::GetResultForContent(nsIDOMElement* aElement,
                                         nsIXULTemplateResult** aResult)
{
    NS_ENSURE_ARG_POINTER(aElement);
    NS_ENSURE_ARG_POINTER(aResult);

    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    if (content == mRoot) {
        *aResult = mRootResult;
    }
    else {
        nsTemplateMatch *match = nullptr;
        if (mContentSupportMap.Get(content, &match))
            *aResult = match->mResult;
        else
            *aResult = nullptr;
    }

    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIDocumentObserver methods
//

void
nsXULContentBuilder::AttributeChanged(nsIDocument* aDocument,
                                      Element*     aElement,
                                      int32_t      aNameSpaceID,
                                      nsIAtom*     aAttribute,
                                      int32_t      aModType)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

    // Handle "open" and "close" cases. We do this handling before
    // we've notified the observer, so that content is already created
    // for the frame system to walk.
    if (aElement->GetNameSpaceID() == kNameSpaceID_XUL &&
        aAttribute == nsGkAtoms::open) {
        // We're on a XUL tag, and an ``open'' attribute changed.
        if (aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                  nsGkAtoms::_true, eCaseMatters))
            OpenContainer(aElement);
        else
            CloseContainer(aElement);
    }

    if ((aNameSpaceID == kNameSpaceID_XUL) &&
        ((aAttribute == nsGkAtoms::sort) ||
         (aAttribute == nsGkAtoms::sortDirection) ||
         (aAttribute == nsGkAtoms::sortResource) ||
         (aAttribute == nsGkAtoms::sortResource2)))
        mSortState.initialized = false;

    // Pass along to the generic template builder.
    nsXULTemplateBuilder::AttributeChanged(aDocument, aElement, aNameSpaceID,
                                           aAttribute, aModType);
}

void
nsXULContentBuilder::NodeWillBeDestroyed(const nsINode* aNode)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    // Break circular references
    mContentSupportMap.Clear();

    nsXULTemplateBuilder::NodeWillBeDestroyed(aNode);
}


//----------------------------------------------------------------------
//
// nsXULTemplateBuilder methods
//

bool
nsXULContentBuilder::GetInsertionLocations(nsIXULTemplateResult* aResult,
                                           nsCOMArray<nsIContent>** aLocations)
{
    *aLocations = nullptr;

    nsAutoString ref;
    nsresult rv = aResult->GetBindingFor(mRefVariable, ref);
    if (NS_FAILED(rv))
        return false;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mRoot->GetDocument());
    if (! xuldoc)
        return false;

    *aLocations = new nsCOMArray<nsIContent>;
    NS_ENSURE_TRUE(*aLocations, false);

    xuldoc->GetElementsForID(ref, **aLocations);
    uint32_t count = (*aLocations)->Count();

    bool found = false;

    for (uint32_t t = 0; t < count; t++) {
        nsCOMPtr<nsIContent> content = (*aLocations)->SafeObjectAt(t);

        nsTemplateMatch* refmatch;
        if (content == mRoot || mContentSupportMap.Get(content, &refmatch)) {
            // See if we've built the container contents for "content"
            // yet. If not, we don't need to build any content. This
            // happens, for example, if we receive an assertion on a
            // closed folder in a tree widget or on a menu that hasn't
            // yet been opened.
            nsXULElement *xulcontent = nsXULElement::FromContent(content);
            if (!xulcontent || xulcontent->GetTemplateGenerated()) {
                found = true;
                continue;
            }
        }

        // clear the item in the list since we don't want to insert there
        (*aLocations)->ReplaceObjectAt(nullptr, t);
    }

    return found;
}

nsresult
nsXULContentBuilder::ReplaceMatch(nsIXULTemplateResult* aOldResult,
                                  nsTemplateMatch* aNewMatch,
                                  nsTemplateRule* aNewMatchRule,
                                  void *aContext)

{
    nsresult rv;
    nsIContent* content = static_cast<nsIContent*>(aContext);

    // update the container attributes for the match
    if (content) {
        nsAutoString ref;
        if (aNewMatch)
            rv = aNewMatch->mResult->GetBindingFor(mRefVariable, ref);
        else
            rv = aOldResult->GetBindingFor(mRefVariable, ref);
        if (NS_FAILED(rv))
            return rv;

        if (!ref.IsEmpty()) {
            nsCOMPtr<nsIXULTemplateResult> refResult;
            rv = GetResultForId(ref, getter_AddRefs(refResult));
            if (NS_FAILED(rv))
                return rv;

            if (refResult)
                SetContainerAttrs(content, refResult, false, true);
        }
    }

    if (aOldResult) {
        nsCOMArray<nsIContent> elements;
        rv = GetElementsForResult(aOldResult, elements);
        if (NS_FAILED(rv))
            return rv;

        uint32_t count = elements.Count();

        for (int32_t e = int32_t(count) - 1; e >= 0; --e) {
            nsCOMPtr<nsIContent> child = elements.SafeObjectAt(e);

            nsTemplateMatch* match;
            if (mContentSupportMap.Get(child, &match)) {
                if (content == match->GetContainer())
                    RemoveMember(child);
            }
        }
    }

    if (aNewMatch) {
        nsCOMPtr<nsIContent> action = aNewMatchRule->GetAction();
        return BuildContentFromTemplate(action, content, content, true,
                                        mRefVariable == aNewMatchRule->GetMemberVariable(),
                                        aNewMatch->mResult, true, aNewMatch,
                                        nullptr, nullptr);
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::SynchronizeResult(nsIXULTemplateResult* aResult)
{
    nsCOMArray<nsIContent> elements;
    GetElementsForResult(aResult, elements);

    uint32_t cnt = elements.Count();

    for (int32_t i = int32_t(cnt) - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> element = elements.SafeObjectAt(i);

        nsTemplateMatch* match;
        if (! mContentSupportMap.Get(element, &match))
            continue;

        nsCOMPtr<nsIContent> templateNode;
        mTemplateMap.GetTemplateFor(element, getter_AddRefs(templateNode));

        NS_ASSERTION(templateNode, "couldn't find template node for element");
        if (! templateNode)
            continue;

        // this node was created by a XUL template, so update it accordingly
        SynchronizeUsingTemplate(templateNode, element, aResult);
    }

    return NS_OK;
}

//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULContentBuilder::OpenContainer(nsIContent* aElement)
{
    if (aElement != mRoot) {
        if (mFlags & eDontRecurse)
            return NS_OK;

        bool rightBuilder = false;

        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(aElement->GetDocument());
        if (! xuldoc)
            return NS_OK;

        // See if we're responsible for this element
        nsIContent* content = aElement;
        do {
            nsCOMPtr<nsIXULTemplateBuilder> builder;
            xuldoc->GetTemplateBuilderFor(content, getter_AddRefs(builder));
            if (builder) {
                if (builder == this)
                    rightBuilder = true;
                break;
            }

            content = content->GetParent();
        } while (content);

        if (! rightBuilder)
            return NS_OK;
    }

    CreateTemplateAndContainerContents(aElement, false);

    return NS_OK;
}

nsresult
nsXULContentBuilder::CloseContainer(nsIContent* aElement)
{
    return NS_OK;
}

nsresult
nsXULContentBuilder::RebuildAll()
{
    NS_ENSURE_TRUE(mRoot, NS_ERROR_NOT_INITIALIZED);

    // Bail out early if we are being torn down.
    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    if (!doc)
        return NS_OK;

    if (mQueriesCompiled)
        Uninit(false);

    nsresult rv = CompileQueries();
    if (NS_FAILED(rv))
        return rv;

    if (mQuerySets.Length() == 0)
        return NS_OK;

    nsXULElement *xulcontent = nsXULElement::FromContent(mRoot);
    if (xulcontent)
        xulcontent->ClearTemplateGenerated();

    // Now, regenerate both the template- and container-generated
    // contents for the current element...
    CreateTemplateAndContainerContents(mRoot, false);

    return NS_OK;
}

/**** Sorting Methods ****/

nsresult
nsXULContentBuilder::CompareResultToNode(nsIXULTemplateResult* aResult,
                                         nsIContent* aContent,
                                         int32_t* aSortOrder)
{
    NS_ASSERTION(aSortOrder, "CompareResultToNode: null out param aSortOrder");
  
    *aSortOrder = 0;

    nsTemplateMatch *match = nullptr;
    if (!mContentSupportMap.Get(aContent, &match)) {
        *aSortOrder = mSortState.sortStaticsLast ? -1 : 1;
        return NS_OK;
    }

    if (!mQueryProcessor)
        return NS_OK;

    if (mSortState.direction == nsSortState_natural) {
        // sort in natural order
        nsresult rv = mQueryProcessor->CompareResults(aResult, match->mResult,
                                                      nullptr, mSortState.sortHints,
                                                      aSortOrder);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        // iterate over each sort key and compare. If the nodes are equal,
        // continue to compare using the next sort key. If not equal, stop.
        int32_t length = mSortState.sortKeys.Count();
        for (int32_t t = 0; t < length; t++) {
            nsresult rv = mQueryProcessor->CompareResults(aResult, match->mResult,
                                                          mSortState.sortKeys[t],
                                                          mSortState.sortHints, aSortOrder);
            NS_ENSURE_SUCCESS(rv, rv);

            if (*aSortOrder)
                break;
        }
    }

    // flip the sort order if performing a descending sorting
    if (mSortState.direction == nsSortState_descending)
        *aSortOrder = -*aSortOrder;

    return NS_OK;
}

nsresult
nsXULContentBuilder::InsertSortedNode(nsIContent* aContainer,
                                      nsIContent* aNode,
                                      nsIXULTemplateResult* aResult,
                                      bool aNotify)
{
    nsresult rv;

    if (!mSortState.initialized) {
        nsAutoString sort, sortDirection, sortHints;
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, sortDirection);
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::sorthints, sortHints);
        sortDirection.AppendLiteral(" ");
        sortDirection += sortHints;
        rv = XULSortServiceImpl::InitializeSortState(mRoot, aContainer,
                                                     sort, sortDirection, &mSortState);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // when doing a natural sort, items will typically be sorted according to
    // the order they appear in the datasource. For RDF, cache whether the
    // reference parent is an RDF Seq. That way, the items can be sorted in the
    // order they are in the Seq.
    mSortState.isContainerRDFSeq = false;
    if (mSortState.direction == nsSortState_natural) {
        nsCOMPtr<nsISupports> ref;
        nsresult rv = aResult->GetBindingObjectFor(mRefVariable, getter_AddRefs(ref));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIRDFResource> container = do_QueryInterface(ref);

        if (container) {
            rv = gRDFContainerUtils->IsSeq(mDB, container, &mSortState.isContainerRDFSeq);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    bool childAdded = false;
    uint32_t numChildren = aContainer->GetChildCount();

    if (mSortState.direction != nsSortState_natural ||
        (mSortState.direction == nsSortState_natural && mSortState.isContainerRDFSeq))
    {
        // because numChildren gets modified
        int32_t realNumChildren = numChildren;
        nsIContent *child = nullptr;

        // rjc says: determine where static XUL ends and generated XUL/RDF begins
        int32_t staticCount = 0;

        nsAutoString staticValue;
        aContainer->GetAttr(kNameSpaceID_None, nsGkAtoms::staticHint, staticValue);
        if (!staticValue.IsEmpty())
        {
            // found "static" XUL element count hint
            nsresult strErr = NS_OK;
            staticCount = staticValue.ToInteger(&strErr);
            if (NS_FAILED(strErr))
                staticCount = 0;
        } else {
            // compute the "static" XUL element count
            for (nsIContent* child = aContainer->GetFirstChild();
                 child;
                 child = child->GetNextSibling()) {
                 
                if (nsContentUtils::HasNonEmptyAttr(child, kNameSpaceID_None,
                                                    nsGkAtoms::_template))
                    break;
                else
                    ++staticCount;
            }

            if (mSortState.sortStaticsLast) {
                // indicate that static XUL comes after RDF-generated content by
                // making negative
                staticCount = -staticCount;
            }

            // save the "static" XUL element count hint
            nsAutoString valueStr;
            valueStr.AppendInt(staticCount);
            aContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::staticHint, valueStr, false);
        }

        if (staticCount <= 0) {
            numChildren += staticCount;
            staticCount = 0;
        } else if (staticCount > (int32_t)numChildren) {
            staticCount = numChildren;
            numChildren -= staticCount;
        }

        // figure out where to insert the node when a sort order is being imposed
        if (numChildren > 0) {
            nsIContent *temp;
            int32_t direction;

            // rjc says: The following is an implementation of a fairly optimal
            // binary search insertion sort... with interpolation at either end-point.

            if (mSortState.lastWasFirst) {
                child = aContainer->GetChildAt(staticCount);
                temp = child;
                rv = CompareResultToNode(aResult, temp, &direction);
                if (direction < 0) {
                    aContainer->InsertChildAt(aNode, staticCount, aNotify);
                    childAdded = true;
                } else
                    mSortState.lastWasFirst = false;
            } else if (mSortState.lastWasLast) {
                child = aContainer->GetChildAt(realNumChildren - 1);
                temp = child;
                rv = CompareResultToNode(aResult, temp, &direction);
                if (direction > 0) {
                    aContainer->InsertChildAt(aNode, realNumChildren, aNotify);
                    childAdded = true;
                } else
                    mSortState.lastWasLast = false;
            }

            int32_t left = staticCount + 1, right = realNumChildren, x;
            while (!childAdded && right >= left) {
                x = (left + right) / 2;
                child = aContainer->GetChildAt(x - 1);
                temp = child;

                rv = CompareResultToNode(aResult, temp, &direction);
                if ((x == left && direction < 0) ||
                    (x == right && direction >= 0) ||
                    left == right)
                {
                    int32_t thePos = (direction > 0 ? x : x - 1);
                    aContainer->InsertChildAt(aNode, thePos, aNotify);
                    childAdded = true;

                    mSortState.lastWasFirst = (thePos == staticCount);
                    mSortState.lastWasLast = (thePos >= realNumChildren);

                    break;
                }
                if (direction < 0)
                    right = x - 1;
                else
                    left = x + 1;
            }
        }
    }

    // if the child hasn't been inserted yet, just add it at the end. Note
    // that an append isn't done as there may be static content afterwards.
    if (!childAdded)
        aContainer->InsertChildAt(aNode, numChildren, aNotify);

    return NS_OK;
}
