/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Neil Deakin <enndeakin@sympatico.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsGkAtoms.h"
#include "nsXULContentUtils.h"
#include "nsXULElement.h"
#include "nsXULTemplateBuilder.h"
#include "nsSupportsArray.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsAttrName.h"
#include "nsNodeUtils.h"

#include "jsapi.h"
#include "pldhash.h"
#include "rdf.h"

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
 * The content builder generates DOM nodes from a template. Generation is done
 * dynamically on demand when child nodes are asked for by some other part of
 * content or layout. This is done for a content node by calling
 * CreateContents which creates one and only one level of children deeper. The
 * next level of content is created by calling CreateContents on each child
 * node when requested.
 *
 * CreateTemplateAndContainerContents is used to determine where in a
 * hierarchy generation is currently, related to the current node being
 * processed. The actual content generation is done entirely inside
 * BuildContentFromTemplate.
 *
 * Content generation is centered around the generation node (the node with
 * uri="?member" on it). Nodes above the generation node are unique and
 * generated only once. BuildContentFromTemplate will be passed the unique
 * flag as an argument for content at this point and will recurse until it
 * finds the generation node.
 *
 * Once the generation node has been found, the results for that content node
 * are added to the content map, stored in mContentSupportMap. When
 * CreateContents is later called for that node, the results are retrieved and
 * used to create a new child for each result, based on the template.
 *
 * Children below the generation node are created with CreateTemplateContents.
 *
 * If recursion is allowed, generation continues, where the generation node
 * becomes the container to insert into.
 *
 * The XUL lazy state bits are used to control some aspects of generation:
 *
 * eChildrenMustBeRebuilt: set to true for a node that has its children
 *                         generated lazily. If this is set, the element will
 *                         need to call into the template builder to generate
 *                         its children. This state is cleared by the element
 *                         when this call is made.
 * eTemplateContentsBuilt: set to true for non-generation nodes if the
 *                         children have already been created.
 * eContainerContentsBuilt: set to true for generation nodes to indicate that
 *                          results have been determined.
 */
class nsXULContentBuilder : public nsXULTemplateBuilder
{
public:
    // nsIXULTemplateBuilder interface
    NS_IMETHOD CreateContents(nsIContent* aElement);

    NS_IMETHOD HasGeneratedContent(nsIRDFResource* aResource,
                                   nsIAtom* aTag,
                                   PRBool* aGenerated);

    NS_IMETHOD GetResultForContent(nsIDOMElement* aContent,
                                   nsIXULTemplateResult** aResult);

    // nsIDocumentObserver interface
    virtual void AttributeChanged(nsIDocument* aDocument,
                                  nsIContent*  aContent,
                                  PRInt32      aNameSpaceID,
                                  nsIAtom*     aAttribute,
                                  PRInt32      aModType);

    void NodeWillBeDestroyed(const nsINode* aNode);

protected:
    friend NS_IMETHODIMP
    NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULContentBuilder();

    void Traverse(nsCycleCollectionTraversalCallback &cb) const
    {
        mSortState.Traverse(cb);
    }

    virtual void Uninit(PRBool aIsFinal);

    // Implementation methods
    nsresult
    OpenContainer(nsIContent* aElement);

    nsresult
    CloseContainer(nsIContent* aElement);

    /**
     * Build content from a template for a given result. This will be called
     * recursively or on demand and will be called for every node in the
     * generated content tree. See the method defintion below for details
     * of arguments.
     */
    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aResourceNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIXULTemplateResult* aChild,
                             PRBool aNotify,
                             nsTemplateMatch* aMatch,
                             nsIContent** aContainer,
                             PRInt32* aNewIndexInContainer);

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
                            PRBool aNotify);

    /**
     * Add any necessary persistent attributes (persist="...") from the
     * local store to a generated node.
     *
     * @param aTemplateNode node within template
     * @param aRealNode generated node to set persisted attibutes upon
     * @param aResult result to look up variable->value bindings in
     */
    nsresult
    AddPersistentAttributes(nsIContent* aTemplateNode,
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
     * CreateTemplateContents and CreateContainerContents. Both of these
     * functions will generate content but under different circumstances.
     *
     * Consider the following example:
     *   <action>
     *     <hbox uri="?node">
     *       <button label="?name"/>
     *     </hbox>
     *   </action>
     *
     * At the top level, CreateTemplateContents will generate nothing, while
     * CreateContainerContents will create an <hbox> for each result. When
     * CreateTemplateAndContainerContents is called for each hbox,
     * CreateTemplateContents will create the buttons, while
     * CreateContainerContents will create the next set of hboxes recursively.
     *
     * Thus, CreateContainerContents creates the nodes with the uri attribute
     * and above, while CreateTemplateContents creates the nodes below that.
     *
     * Note that all content is actually generated inside
     * BuildContentFromTemplate, the various CreateX functions call this in
     * different ways.
     *
     * aContainer will be set to the container in which content was generated.
     * This will always be either aElement, or a descendant of it.
     * aNewIndexInContainer will be the index in this container where content
     * was generated.
     *
     * @param aElement element to generate content inside
     * @param aContainer container content was added inside
     * @param aNewIndexInContainer index with container in which content was added
     */
    nsresult
    CreateTemplateAndContainerContents(nsIContent* aElement,
                                       nsIContent** aContainer,
                                       PRInt32* aNewIndexInContainer);

    /**
     * Generate the results for a template, by calling
     * CreateContainerContentsForQuerySet for each queryset.
     *
     * @param aElement element to generate content inside
     * @param aResult reference point for query
     * @param aNotify true to notify of DOM changes
     * @param aContainer container content was added inside
     * @param aNewIndexInContainer index with container in which content was added
     */
    nsresult
    CreateContainerContents(nsIContent* aElement,
                            nsIXULTemplateResult* aResult,
                            PRBool aNotify,
                            nsIContent** aContainer,
                            PRInt32* aNewIndexInContainer);

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
                                       PRBool aNotify,
                                       nsTemplateQuerySet* aQuerySet,
                                       nsIContent** aContainer,
                                       PRInt32* aNewIndexInContainer);

    /**
     * Create the remaining content for a result. See the description of
     * CreateTemplateAndContainerContents for details. aContainer will be
     * either set to aElement if content was inserted or null otherwise.
     *
     * @param aElement element to generate content inside
     * @param aTemplateElement element within template to generate from
     * @param aContainer container content was added inside
     * @param aNewIndexInContainer index with container in which content was added
     */
    nsresult
    CreateTemplateContents(nsIContent* aElement,
                           nsIContent* aTemplateElement,
                           nsIContent** aContainer,
                           PRInt32* aNewIndexInContainer);

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
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 PRBool aNotify,
                                 nsIContent** aResult);

    PRBool
    IsOpen(nsIContent* aElement);

    nsresult
    RemoveGeneratedContent(nsIContent* aElement);

    PRBool
    IsLazyWidgetItem(nsIContent* aElement);

    nsresult
    GetElementsForResult(nsIXULTemplateResult* aResult,
                         nsCOMArray<nsIContent>& aElements);

    nsresult
    CreateElement(PRInt32 aNameSpaceID,
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
                      PRBool aIgnoreNonContainers,
                      PRBool aNotify);

    virtual nsresult
    RebuildAll();

    // GetInsertionLocations, ReplaceMatch and SynchronizeResult are inherited
    // from nsXULTemplateBuilder

    /**
     * Return true if the result can be inserted into the template as
     * generated content. For the content builder, aLocations will be set
     * to the list of containers where the content should be inserted.
     */
    virtual PRBool
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
                        PRInt32* aSortOrder);

    /**
     * Insert a generated node into the container where it should go according
     * to the current sort. aNode is the generated content node and aResult is
     * the result for the generated node.
     */
    nsresult
    InsertSortedNode(nsIContent* aContainer,
                     nsIContent* aNode,
                     nsIXULTemplateResult* aResult,
                     PRBool aNotify);

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

NS_IMETHODIMP
NS_NewXULContentBuilder(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
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
  mSortState.initialized = PR_FALSE;
}

void
nsXULContentBuilder::Uninit(PRBool aIsFinal)
{
    if (! aIsFinal && mRoot) {
        nsresult rv = RemoveGeneratedContent(mRoot);
        if (NS_FAILED(rv))
            return;
    }

    // Nuke the content support map completely.
    mContentSupportMap.Clear();
    mTemplateMap.Clear();

    mSortState.initialized = PR_FALSE;

    nsXULTemplateBuilder::Uninit(aIsFinal);
}

nsresult
nsXULContentBuilder::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                              nsIContent *aResourceNode,
                                              nsIContent *aRealNode,
                                              PRBool aIsUnique,
                                              nsIXULTemplateResult* aChild,
                                              PRBool aNotify,
                                              nsTemplateMatch* aMatch,
                                              nsIContent** aContainer,
                                              PRInt32* aNewIndexInContainer)
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

        const char *tmpln, *resn, *realn;
        aTemplateNode->Tag()->GetUTF8String(&tmpln);
        aResourceNode->Tag()->GetUTF8String(&resn);
        aRealNode->Tag()->GetUTF8String(&realn);

        nsAutoString id;
        aChild->GetId(id);

        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("Tags: [Template: %s  Resource: %s  Real: %s] for id %s",
               tmpln, resn, realn, NS_ConvertUTF16toUTF8(id).get()));
    }
#endif

    // Iterate through all of the template children, constructing
    // "real" content model nodes for each "template" child.
    PRUint32 count = aTemplateNode->GetChildCount();

    for (PRUint32 kid = 0; kid < count; kid++) {
        nsIContent *tmplKid = aTemplateNode->GetChildAt(kid);

        PRInt32 nameSpaceID = tmplKid->GetNameSpaceID();

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
        PRBool isGenerationElement = PR_FALSE;
        PRBool isUnique = aIsUnique;

        {
            // We identify the resource element by presence of a
            // "uri='rdf:*'" attribute. (We also support the older
            // "uri='...'" syntax.)
            if (tmplKid->HasAttr(kNameSpaceID_None, nsGkAtoms::uri) && aMatch->IsActive()) {
                isGenerationElement = PR_TRUE;
                isUnique = PR_FALSE;
            }
        }

        nsIAtom *tag = tmplKid->Tag();

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
            const char *tagname;
            tag->GetUTF8String(&tagname);
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p]     building %s %s %s",
                    this, tagname,
                    (isGenerationElement ? "[resource]" : ""),
                    (isUnique ? "[unique]" : "")));
        }
#endif

        // Set to PR_TRUE if the child we're trying to create now
        // already existed in the content model.
        PRBool realKidAlreadyExisted = PR_FALSE;

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
                realKidAlreadyExisted = PR_TRUE;
            }
            else {
                // Mark the element's contents as being generated so
                // that any re-entrant calls don't trigger an infinite
                // recursion.
                nsXULElement *xulcontent = nsXULElement::FromContent(realKid);
                if (xulcontent) {
                    xulcontent->SetLazyState(nsXULElement::eTemplateContentsBuilt);
                }

                // Potentially remember the index of this element as the first
                // element that we've generated. Note that we remember
                // this -before- we recurse!
                if (aContainer && !*aContainer) {
                    *aContainer = aRealNode;
                    NS_ADDREF(*aContainer);

                    PRUint32 indx = aRealNode->GetChildCount();

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

            rv = realKid->SetAttr(kNameSpaceID_None, nsGkAtoms::id, id, PR_FALSE);
            if (NS_FAILED(rv))
                return rv;

            if (! aNotify) {
                // XUL document will watch us, and take care of making
                // sure that we get added to or removed from the
                // element map if aNotify is true. If not, we gotta do
                // it ourselves. Yay.
                nsCOMPtr<nsIXULDocument> xuldoc =
                    do_QueryInterface(mRoot->GetDocument());
                if (xuldoc)
                    xuldoc->AddElementForID(id, realKid);
            }

            // Set up the element's 'container' and 'empty' attributes.
            SetContainerAttrs(realKid, aChild, PR_TRUE, PR_FALSE);
        }
        else if (tag == nsGkAtoms::textnode &&
                 nameSpaceID == kNameSpaceID_XUL) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the 'rdf:resource' attribute for the
            // given node.
            // SynchronizeUsingTemplate contains code used to update textnodes,
            // so make sure to modify both when changing this
            PRUnichar attrbuf[128];
            nsFixedString attrValue(attrbuf, NS_ARRAY_LENGTH(attrbuf), 0);
            tmplKid->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);
            if (!attrValue.IsEmpty()) {
                nsAutoString value;
                rv = SubstituteText(aChild, attrValue, value);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> content;
                rv = NS_NewTextNode(getter_AddRefs(content),
                                    mRoot->NodeInfo()->NodeInfoManager());
                if (NS_FAILED(rv)) return rv;

                content->SetText(value, PR_FALSE);

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
            tmplTextNode->CloneNode(PR_FALSE, getter_AddRefs(clonedNode));
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

                PRUint32 indx = aRealNode->GetChildCount();

                // Since we haven't inserted any content yet, our new
                // index in the container will be the current count of
                // elements in the container.
                *aNewIndexInContainer = indx;
            }

            // Remember the template kid from which we created the
            // real kid. This allows us to sync back up with the
            // template to incrementally build content.
            mTemplateMap.Put(realKid, tmplKid);

            rv = CopyAttributesToElement(tmplKid, realKid, aChild, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // Add any persistent attributes
            if (isGenerationElement) {
                rv = AddPersistentAttributes(tmplKid, aChild, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            nsXULElement *xulcontent = nsXULElement::FromContent(realKid);
            if (xulcontent) {
                PRUint32 count2 = tmplKid->GetChildCount();

                if (count2 == 0 && !isGenerationElement) {
                    // If we're at a leaf node, then we'll eagerly
                    // mark the content as having its template &
                    // container contents built. This avoids a useless
                    // trip back to the template builder only to find
                    // that we've got no work to do!
                    xulcontent->SetLazyState(nsXULElement::eTemplateContentsBuilt);
                    xulcontent->SetLazyState(nsXULElement::eContainerContentsBuilt);
                }
                else {
                    // Just mark the XUL element as requiring more work to
                    // be done. We'll get around to it when somebody asks
                    // for it.
                    xulcontent->SetLazyState(nsXULElement::eChildrenMustBeRebuilt);
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

                if (isGenerationElement) {
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
                                             PRBool aNotify)
{
    nsresult rv;

    // Copy all attributes from the template to the new element
    PRUint32 numAttribs = aTemplateNode->GetAttrCount();

    for (PRUint32 attr = 0; attr < numAttribs; attr++) {
        const nsAttrName* name = aTemplateNode->GetAttrNameAt(attr);
        PRInt32 attribNameSpaceID = name->NamespaceID();
        nsIAtom* attribName = name->LocalName();

        // XXXndeakin ignore namespaces until bug 321182 is fixed
        if (attribName != nsGkAtoms::id && attribName != nsGkAtoms::uri) {
            // Create a buffer here, because there's a chance that an
            // attribute in the template is going to be an RDF URI, which is
            // usually longish.
            PRUnichar attrbuf[128];
            nsFixedString attribValue(attrbuf, NS_ARRAY_LENGTH(attrbuf), 0);
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
nsXULContentBuilder::AddPersistentAttributes(nsIContent* aTemplateNode,
                                             nsIXULTemplateResult* aResult,
                                             nsIContent* aRealNode)
{
    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = GetResultResource(aResult, getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    nsAutoString attribute, persist;
    aTemplateNode->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);

    while (!persist.IsEmpty()) {
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

        if (attribute.IsEmpty())
            break;

        nsCOMPtr<nsIAtom> tag;
        PRInt32 nameSpaceID;

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
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> target;
        rv = mDB->GetTarget(resource, property, PR_TRUE, getter_AddRefs(target));
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

        rv = aRealNode->SetAttr(nameSpaceID, tag, nsDependentString(valueStr),
                                PR_FALSE);
        if (NS_FAILED(rv)) return rv;
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
    rv = CopyAttributesToElement(aTemplateNode, aRealElement, aResult, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    // See if we've generated kids for this node yet. If we have, then
    // recursively sync up template kids with content kids
    PRBool contentsGenerated = PR_TRUE;
    nsXULElement *xulcontent = nsXULElement::FromContent(aRealElement);
    if (xulcontent) {
        contentsGenerated = xulcontent->GetLazyState(nsXULElement::eTemplateContentsBuilt);
    }
    else {
        // HTML content will _always_ have been generated up-front
    }

    if (contentsGenerated) {
        PRUint32 count = aTemplateNode->GetChildCount();

        for (PRUint32 loop = 0; loop < count; ++loop) {
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
                nsFixedString attrValue(attrbuf, NS_ARRAY_LENGTH(attrbuf), 0);
                tmplKid->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);
                if (!attrValue.IsEmpty()) {
                    nsAutoString value;
                    rv = SubstituteText(aResult, attrValue, value);
                    if (NS_FAILED(rv)) return rv;
                    realKid->SetText(value, PR_TRUE);
                }
            }

            rv = SynchronizeUsingTemplate(tmplKid, realKid, aResult);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::RemoveMember(nsIContent* aContent)
{
    nsCOMPtr<nsIContent> parent = aContent->GetParent();
    if (parent) {
        PRInt32 pos = parent->IndexOf(aContent);

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) return NS_OK;

        // Note: RemoveChildAt sets |child|'s document to null so that
        // it'll get knocked out of the XUL doc's resource-to-element
        // map.
        nsresult rv = parent->RemoveChildAt(pos, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Remove from the content support map.
    mContentSupportMap.Remove(aContent);

    // Remove from the template map
    mTemplateMap.Remove(aContent);

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateTemplateAndContainerContents(nsIContent* aElement,
                                                        nsIContent** aContainer,
                                                        PRInt32* aNewIndexInContainer)
{
    // Generate both 1) the template content for the current element,
    // and 2) recursive subcontent (if the current element refers to a
    // container result).

    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULContentBuilder::CreateTemplateAndContainerContents start - flags: %d",
            mFlags));

    if (! mQueryProcessor)
        return NS_OK;

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

    // for the root element, get the ref attribute and generate content
    if (aElement == mRoot) {
        if (! mRootResult) {
            nsAutoString ref;
            mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::ref, ref);

            if (! ref.IsEmpty()) {
                nsresult rv = mQueryProcessor->TranslateRef(mDB, ref,
                                                            getter_AddRefs(mRootResult));
                if (NS_FAILED(rv))
                    return rv;
            }
        }

        if (mRootResult) {
            CreateContainerContents(aElement, mRootResult, PR_FALSE,
                                    aContainer, aNewIndexInContainer);
        }
    }
    else if (!(mFlags & eDontRecurse)) {
        // The content map will contain the generation elements (the ones that
        // are given ids) and only those elements, so get the reference point
        // from the corresponding match.
        nsTemplateMatch *match = nsnull;
        if (mContentSupportMap.Get(aElement, &match)) {
            // don't generate children if child processing isn't allowed
            PRBool mayProcessChildren;
            nsresult rv = match->mResult->GetMayProcessChildren(&mayProcessChildren);
            if (NS_FAILED(rv) || !mayProcessChildren)
                return rv;

            CreateContainerContents(aElement, match->mResult, PR_FALSE,
                                    aContainer, aNewIndexInContainer);
        }
    }

    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULContentBuilder::CreateTemplateAndContainerContents end"));

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateContainerContents(nsIContent* aElement,
                                             nsIXULTemplateResult* aResult,
                                             PRBool aNotify,
                                             nsIContent** aContainer,
                                             PRInt32* aNewIndexInContainer)
{
    nsCOMPtr<nsIRDFResource> refResource;
    GetResultResource(aResult, getter_AddRefs(refResource));
    if (! refResource)
        return NS_ERROR_FAILURE;

    // Avoid re-entrant builds for the same resource.
    if (IsActivated(refResource))
        return NS_OK;

    ActivationEntry entry(refResource, &mTop);

    // Create the contents of a container by iterating over all of the
    // "containment" arcs out of the element's resource.
    nsresult rv;

    // Compile the rules now, if they haven't been already.
    if (! mQueriesCompiled) {
        rv = CompileQueries();
        if (NS_FAILED(rv))
            return rv;
    }

    if (mQuerySets.Length() == 0)
        return NS_OK;

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
    nsXULElement *xulcontent = nsXULElement::FromContent(aElement);
    if (xulcontent) {
        if (xulcontent->GetLazyState(nsXULElement::eContainerContentsBuilt))
            return NS_OK;

        // Now mark the element's contents as being generated so that
        // any re-entrant calls don't trigger an infinite recursion.
        xulcontent->SetLazyState(nsXULElement::eContainerContentsBuilt);
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

    PRInt32 querySetCount = mQuerySets.Length();

    for (PRInt32 r = 0; r < querySetCount; r++) {
        nsTemplateQuerySet* queryset = mQuerySets[r];

        nsIAtom* tag = queryset->GetTag();
        if (tag && tag != aElement->Tag())
            continue;

        // XXXndeakin need to revisit how aContainer and content notification
        // is handled. Currently though, this code is similar to the old code.
        // *aContainer will only be set if it is null
        CreateContainerContentsForQuerySet(aElement, aResult, aNotify, queryset,
                                           aContainer, aNewIndexInContainer);
    }

    return NS_OK;
}

nsresult
nsXULContentBuilder::CreateContainerContentsForQuerySet(nsIContent* aElement,
                                                        nsIXULTemplateResult* aResult,
                                                        PRBool aNotify,
                                                        nsTemplateQuerySet* aQuerySet,
                                                        nsIContent** aContainer,
                                                        PRInt32* aNewIndexInContainer)
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
    nsresult rv = mQueryProcessor->GenerateResults(mDB, aResult,
                                                   aQuerySet->mCompiledQuery,
                                                   getter_AddRefs(results));
    if (NS_FAILED(rv) || !results)
        return rv;

    PRBool hasMoreResults;
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
            nsTemplateMatch::Create(mPool, aQuerySet->Priority(),
                                    nextresult, aElement);
        if (!newmatch)
            return NS_ERROR_OUT_OF_MEMORY;

        // check if there is already an existing match. If so, a previous
        // query already generated content so the match is just added to the
        // end of the set of matches.

        PRBool generateContent = PR_TRUE;

        nsTemplateMatch* prevmatch = nsnull;
        nsTemplateMatch* existingmatch = nsnull;
        nsTemplateMatch* removematch = nsnull;
        if (mMatchMap.Get(resultid, &existingmatch)){
            // check if there is an existing match that matched a rule
            while (existingmatch) {
                // break out once we've reached a query in the list with a
                // higher priority, as the new match list is sorted by
                // priority, and the new match should be inserted here
                PRInt32 priority = existingmatch->QuerySetPriority();
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
                        generateContent = PR_FALSE;
                }

                prevmatch = existingmatch;
                existingmatch = existingmatch->mNext;
            }
        }

        if (removematch) {
            // remove the generated content for the existing match
            rv = ReplaceMatch(removematch->mResult, nsnull, nsnull, aElement);
            if (NS_FAILED(rv))
                return rv;
        }

        if (generateContent) {
            // find the rule that matches. If none match, the content does not
            // need to be generated

            PRInt16 ruleindex;
            nsTemplateRule* matchedrule = nsnull;
            rv = DetermineMatchedRule(aElement, nextresult, aQuerySet,
                                      &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(mPool, newmatch);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule,
                                           ruleindex, nextresult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(mPool, newmatch);
                    return rv;
                }

                // Grab the template node
                nsCOMPtr<nsIContent> action;
                matchedrule->GetAction(getter_AddRefs(action));

                BuildContentFromTemplate(action, aElement, aElement, PR_TRUE,
                                         nextresult, aNotify, newmatch,
                                         aContainer, aNewIndexInContainer);
            }
        }

        if (prevmatch) {
            prevmatch->mNext = newmatch;
        }
        else if (!mMatchMap.Put(resultid, newmatch)) {
            nsTemplateMatch::Destroy(mPool, newmatch);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        if (removematch) {
            newmatch->mNext = removematch->mNext;
            nsTemplateMatch::Destroy(mPool, removematch);
        }
        else {
            newmatch->mNext = existingmatch;
        }
    }

    return rv;
}

nsresult
nsXULContentBuilder::CreateTemplateContents(nsIContent* aElement,
                                            nsIContent* aTemplateElement,
                                            nsIContent** aContainer,
                                            PRInt32* aNewIndexInContainer)
{
    // Create the contents of an element using the templates.
    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsXULElement *xulcontent = nsXULElement::FromContent(aElement);
    if (! xulcontent)
        return NS_OK; // HTML content is _always_ generated up-front

    if (xulcontent->GetLazyState(nsXULElement::eTemplateContentsBuilt))
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    xulcontent->SetLazyState(nsXULElement::eTemplateContentsBuilt);

    // Crawl up the content model until we find a generation node
    // (one that was generated from a node with a uri attribute)

    nsTemplateMatch* match = nsnull;
    nsCOMPtr<nsIContent> element;
    for (element = aElement;
         element && element != mRoot;
         element = element->GetParent()) {

        if (mContentSupportMap.Get(element, &match))
            break;
    }

    if (!match)
        return NS_ERROR_FAILURE;

    return BuildContentFromTemplate(aTemplateElement, aElement, aElement,
                                    PR_FALSE, match->mResult, PR_FALSE, match,
                                    aContainer, aNewIndexInContainer);
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

PRBool
nsXULContentBuilder::IsOpen(nsIContent* aElement)
{
    // XXXhyatt - use XBL service to obtain base tag.

    nsIAtom *tag = aElement->Tag();

    // Treat the 'root' element as always open, -unless- it's a
    // menu/menupopup. We don't need to "fake" these as being open.
    if ((aElement == mRoot) && aElement->IsNodeOfType(nsINode::eXUL) &&
        (tag != nsGkAtoms::menu) &&
        (tag != nsGkAtoms::menubutton) &&
        (tag != nsGkAtoms::toolbarbutton) &&
        (tag != nsGkAtoms::button))
      return PR_TRUE;

    return aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                 nsGkAtoms::_true, eCaseMatters);
}

nsresult
nsXULContentBuilder::RemoveGeneratedContent(nsIContent* aElement)
{
    // Keep a queue of "ungenerated" elements that we have to probe
    // for generated content.
    nsAutoVoidArray ungenerated;
    if (!ungenerated.AppendElement(aElement))
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 count;
    while (0 != (count = ungenerated.Count())) {
        // Pull the next "ungenerated" element off the queue.
        PRInt32 last = count - 1;
        nsIContent* element = NS_STATIC_CAST(nsIContent*, ungenerated[last]);
        ungenerated.RemoveElementAt(last);

        PRUint32 i = element->GetChildCount();

        while (i-- > 0) {
            nsCOMPtr<nsIContent> child = element->GetChildAt(i);

            // Optimize for the <template> element, because we *know*
            // it won't have any generated content: there's no reason
            // to even check this subtree.
            // XXX should this check |child| rather than |element|? Otherwise
            //     it should be moved outside the inner loop. Bug 297290.
            if (element->NodeInfo()->Equals(nsGkAtoms::_template,
                                            kNameSpaceID_XUL) ||
                !element->IsNodeOfType(nsINode::eELEMENT))
                continue;

            // If the element is in the template map, then we
            // assume it's been generated and nuke it.
            nsCOMPtr<nsIContent> tmpl;
            mTemplateMap.GetTemplateFor(child, getter_AddRefs(tmpl));

            if (! tmpl) {
                // No 'template' attribute, so this must not have been
                // generated. We'll need to examine its kids.
                if (!ungenerated.AppendElement(child))
                    return NS_ERROR_OUT_OF_MEMORY;
                continue;
            }

            // If we get here, it's "generated". Bye bye!
            element->RemoveChildAt(i, PR_TRUE);

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

    if (!aElement->IsNodeOfType(nsINode::eXUL)) {
        return PR_FALSE;
    }

    // XXXhyatt Use the XBL service to obtain a base tag.

    nsIAtom *tag = aElement->Tag();

    return (tag == nsGkAtoms::menu ||
            tag == nsGkAtoms::menulist ||
            tag == nsGkAtoms::menubutton ||
            tag == nsGkAtoms::toolbarbutton ||
            tag == nsGkAtoms::button ||
            tag == nsGkAtoms::treeitem);
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

    return xuldoc->GetElementsForID(id, aElements);
}

nsresult
nsXULContentBuilder::CreateElement(PRInt32 aNameSpaceID,
                                   nsIAtom* aTag,
                                   nsIContent** aResult)
{
    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc != nsnull, "not initialized");
    if (! doc)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    doc->NodeInfoManager()->GetNodeInfo(aTag, nsnull, aNameSpaceID,
                                        getter_AddRefs(nodeInfo));

    rv = NS_NewElement(getter_AddRefs(result), aNameSpaceID, nodeInfo);
    if (NS_FAILED(rv))
        return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULContentBuilder::SetContainerAttrs(nsIContent *aElement,
                                       nsIXULTemplateResult* aResult,
                                       PRBool aIgnoreNonContainers,
                                       PRBool aNotify)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    PRBool iscontainer;
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
        PRBool isempty;
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
nsXULContentBuilder::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    return CreateTemplateAndContainerContents(aElement, nsnull /* don't care */, nsnull /* don't care */);
}

NS_IMETHODIMP
nsXULContentBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                         nsIAtom* aTag,
                                         PRBool* aGenerated)
{
    *aGenerated = PR_FALSE;

    nsCOMPtr<nsIRDFResource> rootresource;
    nsresult rv = mRootResult->GetResource(getter_AddRefs(rootresource));
    if (NS_FAILED(rv))
        return rv;

    // the root resource is always acceptable
    if (aResource == rootresource) {
        if (!aTag || mRoot->Tag() == aTag)
            *aGenerated = PR_TRUE;
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

        PRUint32 cnt = elements.Count();

        for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
            nsCOMPtr<nsIContent> content = elements.SafeObjectAt(i);

            do {
                nsTemplateMatch* match;
                if (content == mRoot || mContentSupportMap.Get(content, &match)) {
                    // If we've got a tag, check it to ensure we're consistent.
                    if (!aTag || content->Tag() == aTag) {
                        *aGenerated = PR_TRUE;
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
        nsTemplateMatch *match = nsnull;
        if (mContentSupportMap.Get(content, &match))
            *aResult = match->mResult;
        else
            *aResult = nsnull;
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
                                      nsIContent*  aContent,
                                      PRInt32      aNameSpaceID,
                                      nsIAtom*     aAttribute,
                                      PRInt32      aModType)
{
    // Handle "open" and "close" cases. We do this handling before
    // we've notified the observer, so that content is already created
    // for the frame system to walk.
    if ((aContent->GetNameSpaceID() == kNameSpaceID_XUL) &&
        (aAttribute == nsGkAtoms::open)) {
        // We're on a XUL tag, and an ``open'' attribute changed.
        if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                  nsGkAtoms::_true, eCaseMatters))
            OpenContainer(aContent);
        else
            CloseContainer(aContent);
    }

    if ((aNameSpaceID == kNameSpaceID_XUL) &&
        ((aAttribute == nsGkAtoms::sort) ||
         (aAttribute == nsGkAtoms::sortDirection) ||
         (aAttribute == nsGkAtoms::sortResource) ||
         (aAttribute == nsGkAtoms::sortResource2)))
        mSortState.initialized = PR_FALSE;

    // Pass along to the generic template builder.
    nsXULTemplateBuilder::AttributeChanged(aDocument, aContent, aNameSpaceID,
                                           aAttribute, aModType);
}

void
nsXULContentBuilder::NodeWillBeDestroyed(const nsINode* aNode)
{
    // Break circular references
    mContentSupportMap.Clear();

    nsXULTemplateBuilder::NodeWillBeDestroyed(aNode);
}


//----------------------------------------------------------------------
//
// nsXULTemplateBuilder methods
//

PRBool
nsXULContentBuilder::GetInsertionLocations(nsIXULTemplateResult* aResult,
                                           nsCOMArray<nsIContent>** aLocations)
{
    *aLocations = nsnull;

    nsAutoString ref;
    nsresult rv = aResult->GetBindingFor(mRefVariable, ref);
    if (NS_FAILED(rv))
        return PR_FALSE;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mRoot->GetDocument());
    if (! xuldoc)
        return PR_FALSE;

    *aLocations = new nsCOMArray<nsIContent>;
    NS_ENSURE_TRUE(*aLocations, PR_FALSE);

    xuldoc->GetElementsForID(ref, **aLocations);
    PRUint32 count = (*aLocations)->Count();

    PRBool found = PR_FALSE;

    for (PRUint32 t = 0; t < count; t++) {
        nsCOMPtr<nsIContent> content = (*aLocations)->SafeObjectAt(t);

        nsTemplateMatch* refmatch;
        if (content == mRoot || mContentSupportMap.Get(content, &refmatch)) {
            // See if we've built the container contents for "content"
            // yet. If not, we don't need to build any content. This
            // happens, for example, if we receive an assertion on a
            // closed folder in a tree widget or on a menu that hasn't
            // yet been dropped.
            nsXULElement *xulcontent = nsXULElement::FromContent(content);
            if (!xulcontent ||
                xulcontent->GetLazyState(nsXULElement::eContainerContentsBuilt)) {
                // non-XUL content is never built lazily, nor is content that's
                // already been built
                found = PR_TRUE;
                continue;
            }
        }

        // clear the item in the list since we don't want to insert there
        (*aLocations)->ReplaceObjectAt(nsnull, t);
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
    nsIContent* content = NS_STATIC_CAST(nsIContent*, aContext);

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
                SetContainerAttrs(content, refResult, PR_FALSE, PR_TRUE);
        }
    }

    if (aOldResult) {
        nsCOMArray<nsIContent> elements;
        rv = GetElementsForResult(aOldResult, elements);
        if (NS_FAILED(rv))
            return rv;

        PRUint32 count = elements.Count();

        for (PRInt32 e = PRInt32(count) - 1; e >= 0; --e) {
            nsCOMPtr<nsIContent> child = elements.SafeObjectAt(e);

            nsTemplateMatch* match;
            if (mContentSupportMap.Get(child, &match)) {
                if (content == match->GetContainer())
                    RemoveMember(child);
            }
        }
    }

    if (aNewMatch) {
        nsCOMPtr<nsIContent> action;
        aNewMatchRule->GetAction(getter_AddRefs(action));

        return BuildContentFromTemplate(action, content, content, PR_TRUE,
                                        aNewMatch->mResult, PR_TRUE, aNewMatch,
                                        nsnull, nsnull);
    }

    return NS_OK;
}


nsresult
nsXULContentBuilder::SynchronizeResult(nsIXULTemplateResult* aResult)
{
    nsCOMArray<nsIContent> elements;
    GetElementsForResult(aResult, elements);

    PRUint32 cnt = elements.Count();

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
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
    // Get the result for this element if there is one. If it has no result,
    // there's nothing that we need to be concerned about here.
    nsCOMPtr<nsIXULTemplateResult> result;
    if (aElement == mRoot) {
        result = mRootResult;
        if (!result)
            return NS_OK;
    }
    else {
        if (mFlags & eDontRecurse)
            return NS_OK;

        PRBool rightBuilder = PR_FALSE;

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
                    rightBuilder = PR_TRUE;
                break;
            }

            content = content->GetParent();
        } while (content);

        if (! rightBuilder)
            return NS_OK;

        nsTemplateMatch* match;
        if (mContentSupportMap.Get(aElement, &match))
            result = match->mResult;

        if (!result)
            return NS_OK;

        // don't open containers if child processing isn't allowed
        PRBool mayProcessChildren;
        nsresult rv = result->GetMayProcessChildren(&mayProcessChildren);
        if (NS_FAILED(rv) || !mayProcessChildren)
            return rv;
    }

    // The element has a result so build its contents.
    // Create the container's contents "quietly" (i.e., |aNotify ==
    // PR_FALSE|), and then use the |container| and |newIndex| to
    // notify layout where content got created.
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    CreateContainerContents(aElement, result, PR_FALSE, getter_AddRefs(container), &newIndex);

    if (container && IsLazyWidgetItem(aElement)) {
        // The tree widget is special, and has to be spanked every
        // time we add content to a container.
        MOZ_AUTO_DOC_UPDATE(container->GetCurrentDoc(), UPDATE_CONTENT_MODEL,
                            PR_TRUE);
        nsNodeUtils::ContentAppended(container, newIndex);
    }

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
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();

    // Bail out early if we are being torn down.
    if (!doc)
        return NS_OK;

    // See if it's a XUL element whose contents have never even
    // been generated. If so, short-circuit and bail; there's nothing
    // for us to "rebuild" yet. They'll get built correctly the next
    // time somebody asks for them.
    nsXULElement *xulcontent = nsXULElement::FromContent(mRoot);

/*
    // XXXndeakin not sure if commenting this out is a good thing or not.
    // Leaving it in causes templates where the ref is set dynamically to
    // not work due to the order in which these state bits are set.

    if (xulcontent &&
        !xulcontent->GetLazyState(nsXULElement::eContainerContentsBuilt))
        return NS_OK;
*/

    if (mQueriesCompiled)
        Uninit(PR_FALSE);

    nsresult rv = CompileQueries();
    if (NS_FAILED(rv))
        return rv;

    if (mQuerySets.Length() == 0)
        return NS_OK;

    // Forces the XUL element to remember that it needs to
    // re-generate its children next time around.
    if (xulcontent) {
        xulcontent->SetLazyState(nsXULElement::eChildrenMustBeRebuilt);
        xulcontent->ClearLazyState(nsXULElement::eTemplateContentsBuilt);
        xulcontent->ClearLazyState(nsXULElement::eContainerContentsBuilt);
    }

    // Now, regenerate both the template- and container-generated
    // contents for the current element...
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    CreateTemplateAndContainerContents(mRoot, getter_AddRefs(container), &newIndex);

    if (container) {
        MOZ_AUTO_DOC_UPDATE(container->GetCurrentDoc(), UPDATE_CONTENT_MODEL,
                            PR_TRUE);
        nsNodeUtils::ContentAppended(container, newIndex);
    }

    return NS_OK;
}

/**** Sorting Methods ****/

nsresult
nsXULContentBuilder::CompareResultToNode(nsIXULTemplateResult* aResult,
                                         nsIContent* aContent,
                                         PRInt32* aSortOrder)
{
    NS_ASSERTION(aSortOrder, "CompareResultToNode: null out param aSortOrder");
  
    *aSortOrder = 0;

    nsTemplateMatch *match = nsnull;
    if (!mContentSupportMap.Get(aContent, &match)) {
        *aSortOrder = mSortState.sortStaticsLast ? -1 : 1;
        return NS_OK;
    }

    if (!mQueryProcessor)
        return NS_OK;

    if (mSortState.direction == nsSortState_natural) {
        // sort in natural order
        nsresult rv = mQueryProcessor->CompareResults(aResult, match->mResult,
                                                      nsnull, aSortOrder);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        // iterate over each sort key and compare. If the nodes are equal,
        // continue to compare using the next sort key. If not equal, stop.
        PRInt32 length = mSortState.sortKeys.Count();
        for (PRInt32 t = 0; t < length; t++) {
            nsresult rv = mQueryProcessor->CompareResults(aResult, match->mResult,
                                                          mSortState.sortKeys[t], aSortOrder);
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
                                      PRBool aNotify)
{
    nsresult rv;

    if (!mSortState.initialized) {
        nsAutoString sort, sortDirection;
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
        mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::sortDirection, sortDirection);
        rv = XULSortServiceImpl::InitializeSortState(mRoot, aContainer,
                                                     sort, sortDirection, &mSortState);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // when doing a natural sort, items will typically be sorted according to
    // the order they appear in the datasource. For RDF, cache whether the
    // reference parent is an RDF Seq. That way, the items can be sorted in the
    // order they are in the Seq.
    mSortState.isContainerRDFSeq = PR_FALSE;
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

    PRBool childAdded = PR_FALSE;
    PRUint32 numChildren = aContainer->GetChildCount();

    if (mSortState.direction != nsSortState_natural ||
        (mSortState.direction == nsSortState_natural && mSortState.isContainerRDFSeq))
    {
        // because numChildren gets modified
        PRInt32 realNumChildren = numChildren;
        nsIContent *child = nsnull;

        // rjc says: determine where static XUL ends and generated XUL/RDF begins
        PRInt32 staticCount = 0;

        nsAutoString staticValue;
        aContainer->GetAttr(kNameSpaceID_None, nsGkAtoms::staticHint, staticValue);
        if (!staticValue.IsEmpty())
        {
            // found "static" XUL element count hint
            PRInt32 strErr = 0;
            staticCount = staticValue.ToInteger(&strErr);
            if (strErr)
                staticCount = 0;
        } else {
            // compute the "static" XUL element count
            for (PRUint32 childLoop = 0; childLoop < numChildren; ++childLoop) {
                child = aContainer->GetChildAt(childLoop);
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
            aContainer->SetAttr(kNameSpaceID_None, nsGkAtoms::staticHint, valueStr, PR_FALSE);
        }

        if (staticCount <= 0) {
            numChildren += staticCount;
            staticCount = 0;
        } else if (staticCount > (PRInt32)numChildren) {
            staticCount = numChildren;
            numChildren -= staticCount;
        }

        // figure out where to insert the node when a sort order is being imposed
        if (numChildren > 0) {
            nsIContent *temp;
            PRInt32 direction;

            // rjc says: The following is an implementation of a fairly optimal
            // binary search insertion sort... with interpolation at either end-point.

            if (mSortState.lastWasFirst) {
                child = aContainer->GetChildAt(staticCount);
                temp = child;
                rv = CompareResultToNode(aResult, temp, &direction);
                if (direction < 0) {
                    aContainer->InsertChildAt(aNode, staticCount, aNotify);
                    childAdded = PR_TRUE;
                } else
                    mSortState.lastWasFirst = PR_FALSE;
            } else if (mSortState.lastWasLast) {
                child = aContainer->GetChildAt(realNumChildren - 1);
                temp = child;
                rv = CompareResultToNode(aResult, temp, &direction);
                if (direction > 0) {
                    aContainer->InsertChildAt(aNode, realNumChildren, aNotify);
                    childAdded = PR_TRUE;
                } else
                    mSortState.lastWasLast = PR_FALSE;
            }

            PRInt32 left = staticCount + 1, right = realNumChildren, x;
            while (!childAdded && right >= left) {
                x = (left + right) / 2;
                child = aContainer->GetChildAt(x - 1);
                temp = child;

                rv = CompareResultToNode(aResult, temp, &direction);
                if ((x == left && direction < 0) ||
                    (x == right && direction >= 0) ||
                    left == right)
                {
                    PRInt32 thePos = (direction > 0 ? x : x - 1);
                    aContainer->InsertChildAt(aNode, thePos, aNotify);
                    childAdded = PR_TRUE;

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
