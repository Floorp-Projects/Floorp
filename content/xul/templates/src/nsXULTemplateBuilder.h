/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULTemplateBuilder_h__
#define nsXULTemplateBuilder_h__

#include "nsStubDocumentObserver.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContent.h"
#include "nsIObserver.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIXULTemplateBuilder.h"

#include "nsFixedSizeAllocator.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsTemplateRule.h"
#include "nsTemplateMatch.h"
#include "nsIXULTemplateQueryProcessor.h"
#include "nsCycleCollectionParticipant.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

class nsIXULDocument;
class nsIRDFCompositeDataSource;
class nsIObserverService;

/**
 * An object that translates an RDF graph into a presentation using a
 * set of rules.
 */
class nsXULTemplateBuilder : public nsIXULTemplateBuilder,
                             public nsIObserver,
                             public nsStubDocumentObserver
{
    void CleanUp(bool aIsFinal);

public:
    nsXULTemplateBuilder();
    virtual ~nsXULTemplateBuilder();

    nsresult InitGlobals();

    /**
     * Clear the template builder structures. The aIsFinal flag is set to true
     * when the template is going away.
     */
    virtual void Uninit(bool aIsFinal);

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULTemplateBuilder,
                                             nsIXULTemplateBuilder)

    // nsIXULTemplateBuilder interface
    NS_DECL_NSIXULTEMPLATEBUILDER

    // nsIObserver Interface
    NS_DECL_NSIOBSERVER

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    /**
     * Remove an old result and/or add a new result. This method will retrieve
     * the set of containers where the result could be inserted and either add
     * the new result to those containers, or remove the result from those
     * containers. UpdateResultInContainer is called for each container.
     *
     * @param aOldResult result to remove
     * @param aNewResult result to add
     * @param aQueryNode query node for new result
     */
    nsresult
    UpdateResult(nsIXULTemplateResult* aOldResult,
                 nsIXULTemplateResult* aNewResult,
                 nsIDOMNode* aQueryNode);

    /**
     * Remove an old result and/or add a new result from a specific container.
     *
     * @param aOldResult result to remove
     * @param aNewResult result to add
     * @param aQueryNode queryset for the new result
     * @param aOldId id of old result
     * @param aNewId id of new result
     * @param aInsertionPoint container to remove or add result inside
     */
    nsresult
    UpdateResultInContainer(nsIXULTemplateResult* aOldResult,
                            nsIXULTemplateResult* aNewResult,
                            nsTemplateQuerySet* aQuerySet,
                            nsIRDFResource* aOldId,
                            nsIRDFResource* aNewId,
                            nsIContent* aInsertionPoint);

    nsresult
    ComputeContainmentProperties();

    static bool
    IsTemplateElement(nsIContent* aContent);

    virtual nsresult
    RebuildAll() = 0; // must be implemented by subclasses

    void RunnableRebuild() { Rebuild(); }
    void RunnableLoadAndRebuild() {
      Uninit(false);  // Reset results

      nsCOMPtr<nsIDocument> doc = mRoot ? mRoot->GetDocument() : nullptr;
      if (doc) {
        bool shouldDelay;
        LoadDataSources(doc, &shouldDelay);
        if (!shouldDelay) {
          Rebuild();
        }
      }
    }

    // mRoot should not be cleared until after Uninit is finished so that
    // generated content can be removed during uninitialization.
    void UninitFalse() { Uninit(false); mRoot = nullptr; }
    void UninitTrue() { Uninit(true); mRoot = nullptr; }

    /**
     * Find the <template> tag that applies for this builder
     */
    nsresult
    GetTemplateRoot(nsIContent** aResult);

    /**
     * Compile the template's queries
     */
    nsresult
    CompileQueries();

    /**
     * Compile the template given a <template> in aTemplate. This function
     * is called recursively to handle queries inside a queryset. For the
     * outer pass, aIsQuerySet will be false, while the inner pass this will
     * be true.
     *
     * aCanUseTemplate will be set to true if the template's queries could be
     * compiled, and false otherwise. If false, the entire template is
     * invalid.
     *
     * @param aTemplate <template> to compile
     * @param aQuerySet first queryset
     * @param aIsQuerySet true if 
     * @param aPriority the queryset index, incremented when a new one is added
     * @param aCanUseTemplate true if template is valid
     */
    nsresult
    CompileTemplate(nsIContent* aTemplate,
                    nsTemplateQuerySet* aQuerySet,
                    bool aIsQuerySet,
                    PRInt32* aPriority,
                    bool* aCanUseTemplate);

    /**
     * Compile a query using the extended syntax. For backwards compatible RDF
     * syntax where there is no <query>, the <conditions> becomes the query.
     *
     * @param aRuleElement <rule> element
     * @param aActionElement <action> element
     * @param aMemberVariable member variable for the query
     * @param aQuerySet the queryset
     */
    nsresult 
    CompileExtendedQuery(nsIContent* aRuleElement,
                         nsIContent* aActionElement,
                         nsIAtom* aMemberVariable,
                         nsTemplateQuerySet* aQuerySet);

    /**
     * Determine the ref variable and tag from inside a RDF query.
     */
    void DetermineRDFQueryRef(nsIContent* aQueryElement, nsIAtom** tag);

    /**
     * Determine the member variable from inside an action body. It will be
     * the value of the uri attribute on a node.
     */
    already_AddRefed<nsIAtom> DetermineMemberVariable(nsIContent* aElement);

    /**
     * Compile a simple query. A simple query is one that doesn't have a
     * <query> and should use a default query which would normally just return
     * a list of children of the reference point.
     *
     * @param aRuleElement the <rule>
     * @param aQuerySet the query set
     * @param aCanUseTemplate true if the query is valid
     */
    nsresult 
    CompileSimpleQuery(nsIContent* aRuleElement,
                       nsTemplateQuerySet* aQuerySet,
                       bool* aCanUseTemplate);

    /**
     * Compile the <conditions> tag in a rule
     *
     * @param aRule template rule
     * @param aConditions <conditions> element
     */
    nsresult
    CompileConditions(nsTemplateRule* aRule, nsIContent* aConditions);

    /**
     * Compile a <where> tag in a condition. The caller should set
     * *aCurrentCondition to null for the first condition. This value will be
     * updated to point to the new condition before returning. The conditions
     * will be added to the rule aRule by this method.
     *
     * @param aRule template rule
     * @param aCondition <where> element
     * @param aCurrentCondition compiled condition
     */
    nsresult
    CompileWhereCondition(nsTemplateRule* aRule,
                          nsIContent* aCondition,
                          nsTemplateCondition** aCurrentCondition);

    /**
     * Compile the <bindings> for an extended template syntax rule.
     */
    nsresult
    CompileBindings(nsTemplateRule* aRule, nsIContent* aBindings);

    /**
     * Compile a single binding for an extended template syntax rule.
     */
    nsresult
    CompileBinding(nsTemplateRule* aRule, nsIContent* aBinding);

    /**
     * Add automatic bindings for simple rules
     */
    nsresult
    AddSimpleRuleBindings(nsTemplateRule* aRule, nsIContent* aElement);

    static void
    AddBindingsFor(nsXULTemplateBuilder* aSelf,
                   const nsAString& aVariable,
                   void* aClosure);

    /**
     * Load the datasources for the template. shouldDelayBuilding is an out
     * parameter which will be set to true to indicate that content building
     * should not be performed yet as the datasource has not yet loaded. If
     * false, the datasource has already loaded so building can proceed
     * immediately. In the former case, the datasource or query processor
     * should either rebuild the template or update results when the
     * datasource is loaded as needed.
     */
    nsresult
    LoadDataSources(nsIDocument* aDoc, bool* shouldDelayBuilding);

    /**
     * Called by LoadDataSources to load a datasource given a uri list
     * in aDataSource. The list is a set of uris separated by spaces.
     * If aIsRDFQuery is true, then this is for an RDF datasource which
     * causes the method to check for additional flags specific to the
     * RDF processor.
     */
    nsresult
    LoadDataSourceUrls(nsIDocument* aDocument,
                       const nsAString& aDataSources,
                       bool aIsRDFQuery,
                       bool* aShouldDelayBuilding);

    nsresult
    InitHTMLTemplateRoot();

    /**
     * Determine which rule matches a given result. aContainer is used for
     * tag matching and is optional for non-content generating builders.
     * The returned matched rule is always one of the rules owned by the
     * query set aQuerySet.
     *
     * @param aContainer parent where generated content will be inserted
     * @param aResult result to match
     * @param aQuerySet query set to examine the rules of
     * @param aMatchedRule [out] rule that has matched, or null if any.
     * @param aRuleIndex [out] index of the rule
     */
    nsresult
    DetermineMatchedRule(nsIContent* aContainer,
                         nsIXULTemplateResult* aResult,
                         nsTemplateQuerySet* aQuerySet,
                         nsTemplateRule** aMatchedRule,
                         PRInt16 *aRuleIndex);

    // XXX sigh, the string template foo doesn't mix with
    // operator->*() on egcs-1.1.2, so we'll need to explicitly pass
    // "this" and use good ol' fashioned static callbacks.
    void
    ParseAttribute(const nsAString& aAttributeValue,
                   void (*aVariableCallback)(nsXULTemplateBuilder* aThis, const nsAString&, void*),
                   void (*aTextCallback)(nsXULTemplateBuilder* aThis, const nsAString&, void*),
                   void* aClosure);

    nsresult
    SubstituteText(nsIXULTemplateResult* aMatch,
                   const nsAString& aAttributeValue,
                   nsAString& aResult);

    static void
    SubstituteTextAppendText(nsXULTemplateBuilder* aThis, const nsAString& aText, void* aClosure);

    static void
    SubstituteTextReplaceVariable(nsXULTemplateBuilder* aThis, const nsAString& aVariable, void* aClosure);    

    nsresult 
    IsSystemPrincipal(nsIPrincipal *principal, bool *result);

    /**
     * Convenience method which gets a resource for a result. If a result
     * doesn't have a resource set, it will create one from the result's id.
     */
    nsresult GetResultResource(nsIXULTemplateResult* aResult,
                               nsIRDFResource** aResource);

protected:
    nsCOMPtr<nsISupports> mDataSource;
    nsCOMPtr<nsIRDFDataSource> mDB;
    nsCOMPtr<nsIRDFCompositeDataSource> mCompDB;

    /**
     * Circular reference, broken when the document is destroyed.
     */
    nsCOMPtr<nsIContent> mRoot;

    /**
     * The root result, translated from the root element's ref
     */
    nsCOMPtr<nsIXULTemplateResult> mRootResult;

    nsCOMArray<nsIXULBuilderListener> mListeners;

    /**
     * The query processor which generates results
     */
    nsCOMPtr<nsIXULTemplateQueryProcessor> mQueryProcessor;

    /**
     * The list of querysets
     */
    nsTArray<nsTemplateQuerySet *> mQuerySets;

    /**
     * Set to true if the rules have already been compiled
     */
    bool          mQueriesCompiled;

    /**
     * The default reference and member variables.
     */
    nsCOMPtr<nsIAtom> mRefVariable;
    nsCOMPtr<nsIAtom> mMemberVariable;

    /**
     * The match map contains nsTemplateMatch objects, one for each unique
     * match found, keyed by the resource for that match. A particular match
     * will contain a linked list of all of the matches for that unique result
     * id. Only one is active at a time. When a match is retracted, look in
     * the match map, remove it, and apply the next valid match in sequence to
     * make active.
     */
    nsDataHashtable<nsISupportsHashKey, nsTemplateMatch*> mMatchMap;

    /**
     * Fixed size allocator used to allocate matches
     */
    nsFixedSizeAllocator mPool;

public:

    nsFixedSizeAllocator& GetPool() { return mPool; }

protected:
    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*            gRDFService;
    static nsIRDFContainerUtils*     gRDFContainerUtils;
    static nsIScriptSecurityManager* gScriptSecurityManager;
    static nsIPrincipal*             gSystemPrincipal;
    static nsIObserverService*       gObserverService;

    enum {
        eDontTestEmpty = (1 << 0),
        eDontRecurse = (1 << 1),
        eLoggingEnabled = (1 << 2)
    };

    PRInt32 mFlags;

    /**
     * Stack-based helper class to maintain a list of ``activated''
     * resources; i.e., resources for which we are currently building
     * content.
     */
    class ActivationEntry {
    public:
        nsIRDFResource   *mResource;
        ActivationEntry  *mPrevious;
        ActivationEntry **mLink;

        ActivationEntry(nsIRDFResource *aResource, ActivationEntry **aLink)
            : mResource(aResource),
              mPrevious(*aLink),
              mLink(aLink) { *mLink = this; }

        ~ActivationEntry() { *mLink = mPrevious; }
    };

    /**
     * The top of the stack of resources that we're currently building
     * content for.
     */
    ActivationEntry *mTop;

    /**
     * Determine if a resource is currently on the activation stack.
     */
    bool
    IsActivated(nsIRDFResource *aResource);

    /**
     * Returns true if content may be generated for a result, or false if it
     * cannot, for example, if it would be created inside a closed container.
     * Those results will be generated when the container is opened.
     * If false is returned, no content should be generated. Possible 
     * insertion locations may optionally be set for new content, depending on
     * the builder being used. Note that *aLocations or some items within
     * aLocations may be null.
     */
    virtual bool
    GetInsertionLocations(nsIXULTemplateResult* aResult,
                          nsCOMArray<nsIContent>** aLocations) = 0;

    /**
     * Must be implemented by subclasses. Handle removing the generated
     * output for aOldMatch and adding new output for aNewMatch. Either
     * aOldMatch or aNewMatch may be null. aContext is the location returned
     * from the call to MayGenerateResult.
     */
    virtual nsresult
    ReplaceMatch(nsIXULTemplateResult* aOldResult,
                 nsTemplateMatch* aNewMatch,
                 nsTemplateRule* aNewMatchRule,
                 void *aContext) = 0;

    /**
     * Must be implemented by subclasses. Handle change in bound
     * variable values for aResult. aModifiedVars contains the set
     * of variables that have changed.
     * @param aResult the ersult for which variable bindings has changed.
     * @param aModifiedVars the set of variables for which the bindings
     * have changed.
     */
    virtual nsresult
    SynchronizeResult(nsIXULTemplateResult* aResult) = 0;

    /**
     * Output a new match or removed match to the console.
     *
     * @param aId id of the result
     * @param aMatch new or removed match
     * @param aIsNew true for new matched, false for removed matches
     */
    void
    OutputMatchToLog(nsIRDFResource* aId,
                     nsTemplateMatch* aMatch,
                     bool aIsNew);

    virtual void Traverse(nsCycleCollectionTraversalCallback &cb) const
    {
    }

    /**
     * Document that we're observing. Weak ref!
     */
    nsIDocument* mObservedDocument;
};

#endif // nsXULTemplateBuilder_h__
