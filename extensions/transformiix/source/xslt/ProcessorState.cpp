/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999-2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- added code in ::resolveFunctionCall to support the
 *      document() function.
 *
 */

/**
 * Implementation of ProcessorState
 * Much of this code was ported from XSL:P
**/

#include "ProcessorState.h"
#include "XSLTFunctions.h"
#include "FunctionLib.h"
#include "txURIUtils.h"
#include "XMLUtils.h"
#include "XMLDOMUtils.h"
#include "ExprResult.h"
#include "XMLParser.h"
#include "TxLog.h"
#include "txAtoms.h"
#include "txSingleNodeContext.h"
#include "txTokenizer.h"
#include "txVariableMap.h"
#include "XSLTProcessor.h"


DHASH_WRAPPER(txLoadedDocumentsBase, txLoadedDocumentEntry, nsAString&)

nsresult txLoadedDocumentsHash::init(Document* aSourceDocument,
                                     Document* aStyleDocument)
{
    nsresult rv = Init(8);
    NS_ENSURE_SUCCESS(rv, rv);

    mSourceDocument = aSourceDocument;
    mStyleDocument = aStyleDocument;

    if (mSourceDocument) {
        Add(mSourceDocument);
    }
    if (mStyleDocument) {
        Add(mStyleDocument);
    }

    return NS_OK;
}

txLoadedDocumentsHash::~txLoadedDocumentsHash()
{
    if (!mHashTable.ops) {
        return;
    }

    nsAutoString baseURI;
    if (mSourceDocument) {
        mSourceDocument->getBaseURI(baseURI);
        txLoadedDocumentEntry* entry = GetEntry(baseURI);
        if (entry) {
            entry->mDocument = nsnull;
        }
    }
    if (mStyleDocument) {
        mStyleDocument->getBaseURI(baseURI);
        txLoadedDocumentEntry* entry = GetEntry(baseURI);
        if (entry) {
            entry->mDocument = nsnull;
        }
    }
}

void txLoadedDocumentsHash::Add(Document* aDocument)
{
    if (!mHashTable.ops) {
        return;
    }

    nsAutoString baseURI;
    mSourceDocument->getBaseURI(baseURI);
    txLoadedDocumentEntry* entry = AddEntry(baseURI);
    if (entry) {
        entry->mDocument = aDocument;
    }
}

Document* txLoadedDocumentsHash::Get(const nsAString& aURI)
{
    if (!mHashTable.ops) {
        return nsnull;
    }

    txLoadedDocumentEntry* entry = GetEntry(aURI);
    if (entry) {
        return entry->mDocument;
    }

    return nsnull;
}


/**
 * Creates a new ProcessorState for the given XSL document
**/
ProcessorState::ProcessorState(Node* aSourceNode, Document* aXslDocument)
    : mOutputHandler(0),
      mResultHandler(0),
      mOutputHandlerFactory(0),
      mXslKeys(MB_TRUE),
      mDecimalFormats(MB_TRUE),
      mEvalContext(0),
      mLocalVariables(0),
      mGlobalVariableValues(MB_TRUE),
      mRTFDocument(0),
      mSourceNode(aSourceNode)
{
    NS_ASSERTION(aSourceNode, "missing source node");
    NS_ASSERTION(aXslDocument, "missing xslt document");

    Document* sourceDoc;
    if (mSourceNode->getNodeType() == Node::DOCUMENT_NODE) {
        sourceDoc = (Document*)mSourceNode;
    }
    else {
        sourceDoc = mSourceNode->getOwnerDocument();
    }
    mLoadedDocuments.init(sourceDoc, aXslDocument);

    /* turn object deletion on for some of the Maps (NamedMap) */
    mExprHashes[SelectAttr].setOwnership(Map::eOwnsItems);
    mExprHashes[TestAttr].setOwnership(Map::eOwnsItems);
    mExprHashes[ValueAttr].setOwnership(Map::eOwnsItems);
    mPatternHashes[CountAttr].setOwnership(Map::eOwnsItems);
    mPatternHashes[FromAttr].setOwnership(Map::eOwnsItems);
}

/**
 * Destroys this ProcessorState
**/
ProcessorState::~ProcessorState()
{
  // Delete all ImportFrames
  txListIterator iter(&mImportFrames);
  while (iter.hasNext())
      delete (ImportFrame*)iter.next();

  // in module the outputhandler is refcounted
#ifdef TX_EXE
  delete mOutputHandler;
#endif
} //-- ~ProcessorState


/*
 * Adds the given attribute set to the list of available named attribute
 * sets
 * @param aAttributeSet the Element to add as a named attribute set
 * @param aImportFrame  ImportFrame to add the attributeset to
 */
void ProcessorState::addAttributeSet(Element* aAttributeSet,
                                     ImportFrame* aImportFrame)
{
    if (!aAttributeSet)
        return;

    nsAutoString nameStr;
    txExpandedName name;
    aAttributeSet->getAttr(txXSLTAtoms::name, kNameSpaceID_None, nameStr);
    nsresult rv = name.init(nameStr, aAttributeSet, MB_FALSE);
    if (NS_FAILED(rv)) {
        receiveError(NS_LITERAL_STRING("missing or malformed name for xsl:attribute-set"));
        return;
    }
    // Get attribute set, if already exists, then merge
    NodeSet* attSet = (NodeSet*)aImportFrame->mNamedAttributeSets.get(name);
    if (!attSet) {
        attSet = new NodeSet();
        aImportFrame->mNamedAttributeSets.add(name, attSet);
    }

    // Add xsl:attribute elements to attSet
    Node* node = aAttributeSet->getFirstChild();
    while (node) {
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            PRInt32 nsID = node->getNamespaceID();
            if (nsID != kNameSpaceID_XSLT)
                continue;
            nsCOMPtr<nsIAtom> nodeName;
            if (!node->getLocalName(getter_AddRefs(nodeName)) || !nodeName)
                continue;
            if (nodeName == txXSLTAtoms::attribute)
                attSet->append(node);
        }
        node = node->getNextSibling();
    }

}
/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void ProcessorState::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

/**
 * Adds the given template to the list of templates to process
 * @param xslTemplate  The Element to add as a template
 * @param importFrame  ImportFrame to add the template to
**/
void ProcessorState::addTemplate(Element* aXslTemplate,
                                 ImportFrame* aImportFrame)
{
    NS_ASSERTION(aXslTemplate, "missing template");

    nsresult rv = NS_OK;
    nsAutoString nameStr;
    if (aXslTemplate->getAttr(txXSLTAtoms::name,
                              kNameSpaceID_None, nameStr)) {
        txExpandedName name;
        rv = name.init(nameStr, aXslTemplate, MB_FALSE);
        if (NS_FAILED(rv)) {
            receiveError(NS_LITERAL_STRING("missing or malformed template name: '") +
                         nameStr + NS_LITERAL_STRING("'"), NS_ERROR_FAILURE);
            return;
        }

        rv = aImportFrame->mNamedTemplates.add(name, aXslTemplate);
        if (NS_FAILED(rv)) {
            receiveError(NS_LITERAL_STRING("Unable to add template named '") +
                         nameStr +
                         NS_LITERAL_STRING("'. Does that name already exist?"),
                         NS_ERROR_FAILURE);
            return;
        }
    }

    nsAutoString match;
    if (!aXslTemplate->getAttr(txXSLTAtoms::match, kNameSpaceID_None, match)) {
        // This is no error, see section 6 Named Templates
        return;
    }

    // get the txList for the right mode
    nsAutoString modeStr;
    txExpandedName mode;
    if (aXslTemplate->getAttr(txXSLTAtoms::mode, kNameSpaceID_None, modeStr)) {
        rv = mode.init(modeStr, aXslTemplate, MB_FALSE);
        if (NS_FAILED(rv)) {
            receiveError(NS_LITERAL_STRING("malformed template-mode name: '") +
                         modeStr + NS_LITERAL_STRING("'"), NS_ERROR_FAILURE);
            return;
        }
    }
    txList* templates =
        (txList*)aImportFrame->mMatchableTemplates.get(mode);

    if (!templates) {
        templates = new txList;
        if (!templates) {
            NS_ASSERTION(0, "out of memory");
            return;
        }
        rv = aImportFrame->mMatchableTemplates.add(mode, templates);
        if (NS_FAILED(rv)) {
            delete templates;
            return;
        }
    }

    // Check for explicit default priority
    MBool hasPriority;
    double priority;
    nsAutoString prio;
    if ((hasPriority =
         aXslTemplate->getAttr(txXSLTAtoms::priority, kNameSpaceID_None,
                               prio))) {
        priority = Double::toDouble(prio);
    }

    // Get the pattern
    txPSParseContext context(this, aXslTemplate);
    txPattern* pattern = txPatternParser::createPattern(match, &context, this);
#ifdef TX_PATTERN_DEBUG
    nsAutoString foo;
    pattern->toString(foo);
#endif

    if (!pattern) {
        return;
    }

    // Add the simple patterns to the list of matchable templates, according
    // to default priority
    txList simpleMatches;
    pattern->getSimplePatterns(simpleMatches);
    txListIterator simples(&simpleMatches);
    while (simples.hasNext()) {
        txPattern* simple = (txPattern*)simples.next();
        if (simple != pattern && pattern) {
            // txUnionPattern, it doesn't own the txLocPathPatterns no more,
            // so delete it. (only once, of course)
            delete pattern;
            pattern = 0;
        }
        if (!hasPriority) {
            priority = simple->getDefaultPriority();
        }
        MatchableTemplate* nt = new MatchableTemplate(aXslTemplate,
                                                      simple,
                                                      priority);
        if (!nt) {
            NS_ASSERTION(0, "out of mem");
            return;
        }
        txListIterator templ(templates);
        MBool isLast = MB_TRUE;
        while (templ.hasNext() && isLast) {
            MatchableTemplate* mt = (MatchableTemplate*)templ.next();
            if (priority < mt->mPriority) {
                continue;
            }
            templ.addBefore(nt);
            isLast = MB_FALSE;
        }
        if (isLast)
            templates->add(nt);
    }
}

/*
 * Adds the given LRE Stylesheet to the list of templates to process
 * @param aStylesheet  The Stylesheet to add as a template
 * @param importFrame  ImportFrame to add the template to
 */
void ProcessorState::addLREStylesheet(Document* aStylesheet,
                                      ImportFrame* aImportFrame)
{
    NS_ASSERTION(aStylesheet, "missing stylesheet");
    
    // get the txList for null mode
    txExpandedName nullMode;
    txList* templates =
        (txList*)aImportFrame->mMatchableTemplates.get(nullMode);

    if (!templates) {
        templates = new txList;
        if (!templates) {
            // XXX ErrorReport: out of memory
            return;
        }
        aImportFrame->mMatchableTemplates.add(nullMode, templates);
    }

    // Add the template to the list of templates
    txPattern* root = new txRootPattern(MB_TRUE);
    MatchableTemplate* nt = 0;
    if (root) 
        nt = new MatchableTemplate(aStylesheet, root, 0.5);
    if (!nt) {
        delete root;
        // XXX ErrorReport: out of memory
        return;
    }
    txListIterator templ(templates);
    MBool isLast = MB_TRUE;
    while (templ.hasNext() && isLast) {
        MatchableTemplate* mt = (MatchableTemplate*)templ.next();
        if (0.5 < mt->mPriority) {
            continue;
        }
        templ.addBefore(nt);
        isLast = MB_FALSE;
    }
    if (isLast)
        templates->add(nt);
}

/*
 * Retrieve the document designated by the URI uri, using baseUri as base URI.
 * Parses it as an XML document, and returns it. If a fragment identifier is
 * supplied, the element with seleced id is returned.
 * The returned document is owned by the ProcessorState
 *
 * @param uri the URI of the document to retrieve
 * @param baseUri the base URI used to resolve the URI if uri is relative
 * @return loaded document or element pointed to by fragment identifier. If
 *         loading or parsing fails NULL will be returned.
 */
Node* ProcessorState::retrieveDocument(const nsAString& uri,
                                       const nsAString& baseUri)
{
    nsAutoString absUrl;
    URIUtils::resolveHref(uri, baseUri, absUrl);

    PRInt32 hash = absUrl.RFindChar(PRUnichar('#'));
    PRUint32 urlEnd, fragStart, fragEnd;
    if (hash == kNotFound) {
        urlEnd = absUrl.Length();
        fragStart = 0;
        fragEnd = 0;
    }
    else {
        urlEnd = hash;
        fragStart = hash + 1;
        fragEnd = absUrl.Length();
    }

    nsDependentSubstring docUrl(absUrl, 0, urlEnd);
    nsDependentSubstring frag(absUrl, fragStart, fragEnd);

    PR_LOG(txLog::xslt, PR_LOG_DEBUG,
           ("Retrieve Document %s, uri %s, baseUri %s, fragment %s\n", 
            NS_LossyConvertUCS2toASCII(docUrl).get(),
            NS_LossyConvertUCS2toASCII(uri).get(),
            NS_LossyConvertUCS2toASCII(baseUri).get(),
            NS_LossyConvertUCS2toASCII(frag).get()));

    // try to get already loaded document
    Document* xmlDoc = mLoadedDocuments.Get(docUrl);

    if (!xmlDoc) {
        // open URI
        nsAutoString errMsg;
        XMLParser xmlParser;

        xmlDoc = xmlParser.getDocumentFromURI(docUrl,
                                              mLoadedDocuments.mStyleDocument,
                                              errMsg);

        if (!xmlDoc) {
            receiveError(NS_LITERAL_STRING("Couldn't load document '") +
                         docUrl + NS_LITERAL_STRING("': ") + errMsg,
                         NS_ERROR_XSLT_INVALID_URL);
            return NULL;
        }
        // add to list of documents
        mLoadedDocuments.Add(xmlDoc);
    }

    // return element with supplied id if supplied
    if (!frag.IsEmpty())
        return xmlDoc->getElementById(frag);

    return xmlDoc;
}

/*
 * Return list of import containers
 */
List* ProcessorState::getImportFrames()
{
    return &mImportFrames;
}

/*
 * Find template in specified mode matching the supplied node. Only search
 * templates imported by a specific ImportFrame
 * @param aNode        node to find matching template for
 * @param aMode        mode of the template
 * @param aImportedBy  seach only templates imported by this ImportFrame,
 *                     or null to search all templates
 * @param aImportFrame out-param, is set to the ImportFrame containing
 *                     the found template
 * @return             root-node of found template, null if none is found
 */
Node* ProcessorState::findTemplate(Node* aNode,
                                   const txExpandedName& aMode,
                                   ImportFrame* aImportedBy,
                                   ImportFrame** aImportFrame)
{
    NS_ASSERTION(aImportFrame, "missing ImportFrame pointer");
    NS_ASSERTION(aNode, "missing node");

    if (!aNode)
        return 0;

    Node* matchTemplate = 0;
    ImportFrame* endFrame = 0;
    txListIterator frameIter(&mImportFrames);

    if (aImportedBy) {
        ImportFrame* curr = (ImportFrame*)frameIter.next();
        while (curr != aImportedBy)
               curr = (ImportFrame*)frameIter.next();

        endFrame = aImportedBy->mFirstNotImported;
    }

    ImportFrame* frame;
    while (!matchTemplate &&
           (frame = (ImportFrame*)frameIter.next()) &&
           frame != endFrame) {

        // get templatelist for this mode
        txList* templates;
        templates = (txList*)frame->mMatchableTemplates.get(aMode);

        if (templates) {
            txListIterator templateIter(templates);

            // Find template with highest priority
            MatchableTemplate* templ;
            while (!matchTemplate &&
                   (templ = (MatchableTemplate*)templateIter.next())) {
#ifdef TX_PATTERN_DEBUG
                nsAutoString foo;
                templ->mMatch->toString(foo);
#endif
                if (templ->mMatch->matches(aNode, this)) {
                    matchTemplate = templ->mTemplate;
                    *aImportFrame = frame;
                }
            }
        }
    }

#ifdef PR_LOGGING
    nsAutoString mode, nodeName;
    if (aMode.mLocalName) {
        aMode.mLocalName->ToString(mode);
    }
    aNode->getNodeName(nodeName);
    if (matchTemplate) {
        nsAutoString matchAttr;
        // matchTemplate can be a document (see addLREStylesheet)
        unsigned short nodeType = matchTemplate->getNodeType();
        if (nodeType == Node::ELEMENT_NODE) {
            ((Element*)matchTemplate)->getAttr(txXSLTAtoms::match,
                                               kNameSpaceID_None,
                                               matchAttr);
        }
        nsAutoString baseURI;
        matchTemplate->getBaseURI(baseURI);
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("MatchTemplate, Pattern %s, Mode %s, Stylesheet %s, " \
                "Node %s\n",
                NS_LossyConvertUCS2toASCII(matchAttr).get(),
                NS_LossyConvertUCS2toASCII(mode).get(),
                NS_LossyConvertUCS2toASCII(baseURI).get(),
                NS_LossyConvertUCS2toASCII(nodeName).get()));
    }
    else {
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("No match, Node %s, Mode %s\n", 
                NS_LossyConvertUCS2toASCII(nodeName).get(),
                NS_LossyConvertUCS2toASCII(mode).get()));
    }
#endif
    return matchTemplate;
}

/*
 * Gets current template rule
 */
ProcessorState::TemplateRule* ProcessorState::getCurrentTemplateRule()
{
    return mCurrentTemplateRule;
}

/*
 * Sets current template rule
 */
void ProcessorState::setCurrentTemplateRule(TemplateRule* aTemplateRule)
{
    mCurrentTemplateRule = aTemplateRule;
}

/**
 * Returns the AttributeSet associated with the given name
 * or null if no AttributeSet is found
 */
NodeSet* ProcessorState::getAttributeSet(const txExpandedName& aName)
{
    NodeSet* attset = new NodeSet;
    if (!attset)
        return attset;

    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);
    frameIter.resetToEnd();

    while ((frame = (ImportFrame*)frameIter.previous())) {
        NodeSet* nodes = (NodeSet*)frame->mNamedAttributeSets.get(aName);
        if (nodes)
            attset->append(nodes);
    }
    return attset;
}

Expr* ProcessorState::getExpr(Element* aElem, ExprAttr aAttr)
{
    NS_ASSERTION(aElem, "missing element while getting expression");

    Expr* expr = (Expr*)mExprHashes[aAttr].get(aElem);
    if (expr) {
        return expr;
    }
    nsAutoString attr;
    MBool hasAttr = MB_FALSE;
    switch (aAttr) {
        case SelectAttr:
            hasAttr = aElem->getAttr(txXSLTAtoms::select, kNameSpaceID_None,
                                     attr);
            break;
        case TestAttr:
            hasAttr = aElem->getAttr(txXSLTAtoms::test, kNameSpaceID_None,
                                     attr);
            break;
        case ValueAttr:
            hasAttr = aElem->getAttr(txXSLTAtoms::value, kNameSpaceID_None,
                                     attr);
            break;
    }

    if (!hasAttr)
        return 0;

    txPSParseContext pContext(this, aElem);
    expr = ExprParser::createExpr(attr, &pContext);

    if (!expr) {
        receiveError(NS_LITERAL_STRING("Error in parsing XPath expression: ") +
                     attr, NS_ERROR_XPATH_PARSE_FAILED);
    }
    else {
        mExprHashes[aAttr].put(aElem, expr);
    }
    return expr;
}

txPattern* ProcessorState::getPattern(Element* aElem, PatternAttr aAttr)
{
    NS_ASSERTION(aElem, "missing element while getting pattern");

    txPattern* pattern = (txPattern*)mPatternHashes[aAttr].get(aElem);
    if (pattern) {
        return pattern;
    }
    nsAutoString attr;
    MBool hasAttr = MB_FALSE;
    switch (aAttr) {
        case CountAttr:
            hasAttr = aElem->getAttr(txXSLTAtoms::count, kNameSpaceID_None,
                                     attr);
            break;
        case FromAttr:
            hasAttr = aElem->getAttr(txXSLTAtoms::from, kNameSpaceID_None,
                                     attr);
            break;
    }

    if (!hasAttr)
        return 0;

    
    txPSParseContext pContext(this, aElem);
    pattern = txPatternParser::createPattern(attr, &pContext, this);

    if (!pattern) {
        receiveError(NS_LITERAL_STRING("Error in parsing pattern: ") + attr,
                     NS_ERROR_XPATH_PARSE_FAILED);
    }
    else {
        mPatternHashes[aAttr].put(aElem, pattern);
    }
    return pattern;
}

/*
 * Returns the template associated with the given name, or
 * null if not template is found
 */
Element* ProcessorState::getNamedTemplate(const txExpandedName& aName)
{
    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);

    while ((frame = (ImportFrame*)frameIter.next())) {
        Element* templ = (Element*)frame->mNamedTemplates.get(aName);
        if (templ)
            return templ;
    }
    return 0;
}

txOutputFormat* ProcessorState::getOutputFormat()
{
    return &mOutputFormat;
}

Document* ProcessorState::getRTFDocument()
{
    return mRTFDocument;
}

void ProcessorState::setRTFDocument(Document* aDoc)
{
    mRTFDocument = aDoc;
}

Document* ProcessorState::getStylesheetDocument()
{
    NS_ASSERTION(mLoadedDocuments.mStyleDocument,
                 "missing stylesheet document");
    return mLoadedDocuments.mStyleDocument;
}

/*
 * Add a global variable
 */
nsresult ProcessorState::addGlobalVariable(const txExpandedName& aVarName,
                                           Element* aVarElem,
                                           ImportFrame* aImportFrame,
                                           ExprResult* aDefaultValue)
{
    // If we don't know the value, it's a plain global var, add it
    // to the import frame for late evaluation.
    if (!aDefaultValue) {
        return aImportFrame->mVariables.add(aVarName, aVarElem);
    }
    // Otherwise, add a GlobalVariableValue not owning the value.
    GlobalVariableValue* var =
        (GlobalVariableValue*)mGlobalVariableValues.get(aVarName);
    if (var) {
        // we set this parameter twice, we should set it to the same
        // value;
        NS_ENSURE_TRUE(var->mValue == aDefaultValue, NS_ERROR_UNEXPECTED);
        return NS_OK;
    }
    var = new GlobalVariableValue(aDefaultValue);
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    return mGlobalVariableValues.add(aVarName, var);
}

/*
 * Returns map on top of the stack of local variable-bindings
 */
txVariableMap* ProcessorState::getLocalVariables()
{
    return mLocalVariables;
}

/*
 * Sets top map of the local variable-bindings stack
 */
void ProcessorState::setLocalVariables(txVariableMap* aMap)
{
    mLocalVariables = aMap;
}

void ProcessorState::processAttrValueTemplate(const nsAFlatString& aAttValue,
                                              Element* aContext,
                                              nsAString& aResult)
{
    aResult.Truncate();
    txPSParseContext pContext(this, aContext);
    AttributeValueTemplate* avt =
        ExprParser::createAttributeValueTemplate(aAttValue, &pContext);

    if (!avt) {
        // fallback, just copy the attribute
        aResult.Append(aAttValue);
        return;
    }

    ExprResult* exprResult = avt->evaluate(this->getEvalContext());
    delete avt;
    if (!exprResult) {
        // XXX ErrorReport: out of memory
        return;
    }

    exprResult->stringValue(aResult);
    delete exprResult;
}

/**
 * Adds the set of names to the Whitespace handling list.
 * xsl:strip-space calls this with MB_TRUE, xsl:preserve-space 
 * with MB_FALSE
 */
void ProcessorState::shouldStripSpace(const nsAString& aNames,
                                      Element* aElement,
                                      MBool aShouldStrip,
                                      ImportFrame* aImportFrame)
{
    //-- split names on whitespace
    txTokenizer tokenizer(PromiseFlatString(aNames));
    while (tokenizer.hasMoreTokens()) {
        const nsAString& name = tokenizer.nextToken();
        PRInt32 aNSID = kNameSpaceID_None;
        nsCOMPtr<nsIAtom> prefix;
        XMLUtils::getPrefix(name, getter_AddRefs(prefix));
        if (prefix) {
            aNSID = aElement->lookupNamespaceID(prefix);
        }
        nsCOMPtr<nsIAtom> localName;
        XMLUtils::getLocalPart(name, getter_AddRefs(localName));
        txNameTestItem* nti = new txNameTestItem(prefix, localName,
                                                 aNSID, aShouldStrip);
        if (!nti) {
            // XXX error report, parsing error or out of mem
            break;
        }
        double priority = nti->getDefaultPriority();
        txListIterator iter(&aImportFrame->mWhiteNameTests);
        while (iter.hasNext()) {
            txNameTestItem* iNameTest = (txNameTestItem*)iter.next();
            if (iNameTest->getDefaultPriority() <= priority) {
                break;
            }
        }
        iter.addBefore(nti);
    }

} //-- stripSpace

/**
 * Adds the supplied xsl:key to the set of keys
**/
MBool ProcessorState::addKey(Element* aKeyElem)
{
    nsresult rv = NS_OK;
    nsAutoString keyQName;
    aKeyElem->getAttr(txXSLTAtoms::name, kNameSpaceID_None, keyQName);
    txExpandedName keyName;
    rv = keyName.init(keyQName, aKeyElem, MB_FALSE);
    if (NS_FAILED(rv))
        return MB_FALSE;

    txXSLKey* xslKey = (txXSLKey*)mXslKeys.get(keyName);
    if (!xslKey) {
        xslKey = new txXSLKey(this);
        if (!xslKey)
            return MB_FALSE;
        rv = mXslKeys.add(keyName, xslKey);
        if (NS_FAILED(rv))
            return MB_FALSE;
    }
    txPattern* match = 0;
    txPSParseContext pContext(this, aKeyElem);
    nsAutoString attrVal;
    if (aKeyElem->getAttr(txXSLTAtoms::match, kNameSpaceID_None, attrVal)) {
        match = txPatternParser::createPattern(attrVal, &pContext, this);
    }
    Expr* use = 0;
    attrVal.Truncate();
    if (aKeyElem->getAttr(txXSLTAtoms::use, kNameSpaceID_None, attrVal)) {
        use = ExprParser::createExpr(attrVal, &pContext);
    }
    if (!match || !use || !xslKey->addKey(match, use)) {
        delete match;
        delete use;
        return MB_FALSE;
    }
    return MB_TRUE;
}

/**
 * Adds the supplied xsl:key to the set of keys
 * returns NULL if no such key exists
**/
txXSLKey* ProcessorState::getKey(txExpandedName& keyName)
{
    return (txXSLKey*)mXslKeys.get(keyName);
}

/*
 * Adds a decimal format. Returns false if the format already exists
 * but dosn't contain the exact same parametervalues
 */
MBool ProcessorState::addDecimalFormat(Element* element)
{
    // build new DecimalFormat structure
    nsresult rv = NS_OK;
    MBool success = MB_TRUE;
    txDecimalFormat* format = new txDecimalFormat;
    if (!format)
        return MB_FALSE;

    nsAutoString formatNameStr, attValue;
    txExpandedName formatName;
    if (element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                         formatNameStr)) {
        rv = formatName.init(formatNameStr, element, MB_FALSE);
        if (NS_FAILED(rv))
            return MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::decimalSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mDecimalSeparator = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::groupingSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mGroupingSeparator = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::infinity,
                         kNameSpaceID_None, attValue))
        format->mInfinity=attValue;

    if (element->getAttr(txXSLTAtoms::minusSign,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mMinusSign = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::NaN, kNameSpaceID_None,
                         attValue))
        format->mNaN=attValue;
        
    if (element->getAttr(txXSLTAtoms::percent, kNameSpaceID_None,
                         attValue)) {
        if (attValue.Length() == 1)
            format->mPercent = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::perMille,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mPerMille = attValue.CharAt(0);
        else if (!attValue.IsEmpty())
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::zeroDigit,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mZeroDigit = attValue.CharAt(0);
        else if (!attValue.IsEmpty())
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::digit, kNameSpaceID_None,
                         attValue)) {
        if (attValue.Length() == 1)
            format->mDigit = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::patternSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.Length() == 1)
            format->mPatternSeparator = attValue.CharAt(0);
        else
            success = MB_FALSE;
    }

    if (!success) {
        delete format;
        return MB_FALSE;
    }

    // Does an existing format with that name exist?
    txDecimalFormat* existing =
        (txDecimalFormat*)mDecimalFormats.get(formatName);
    if (existing) {
        success = existing->isEqual(format);
        delete format;
    }
    else {
        rv = mDecimalFormats.add(formatName, format);
        if (NS_FAILED(rv)) {
            delete format;
            success = MB_FALSE;
        }
    }
    
    return success;
}

/*
 * Returns a decimal format or NULL if no such format exists.
 */
txDecimalFormat* ProcessorState::getDecimalFormat(const txExpandedName& aName)
{
    txDecimalFormat* format = (txDecimalFormat*)mDecimalFormats.get(aName);
    if (!format && !aName.mLocalName &&
        aName.mNamespaceID == kNameSpaceID_None)
        return &mDefaultDecimalFormat;
    return format;
}

/**
 * Returns the value of a given variable binding within the current scope
 * @param the name to which the desired variable value has been bound
 * @return the ExprResult which has been bound to the variable with the given
 * name
**/
nsresult ProcessorState::getVariable(PRInt32 aNamespace, nsIAtom* aLName,
                                     ExprResult*& aResult)
{
    nsresult rv;
    aResult = 0;
    ExprResult* exprResult;
    txExpandedName varName(aNamespace, aLName);
    
    // Check local variables
    if (mLocalVariables) {
        exprResult = mLocalVariables->getVariable(varName);
        if (exprResult) {
            aResult = exprResult;
            return NS_OK;
        }
    }

    // Check if global variable is already evaluated
    GlobalVariableValue* globVar;
    globVar = (GlobalVariableValue*)mGlobalVariableValues.get(varName);
    if (globVar) {
        if (globVar->mFlags == GlobalVariableValue::evaluating) {
            receiveError(NS_LITERAL_STRING("Cyclic variable-value detected"),
                         NS_ERROR_FAILURE);
            return NS_ERROR_FAILURE;
        }
        aResult = globVar->mValue;
        return NS_OK;
    }

    // We need to evaluate the variable

    // Search ImportFrames for the variable
    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);
    Element* varElem = 0;
    while (!varElem && (frame = (ImportFrame*)frameIter.next()))
        varElem = (Element*)frame->mVariables.get(varName);

    if (!varElem)
        return NS_ERROR_FAILURE;

    // Evaluate the variable
    globVar = new GlobalVariableValue();
    if (!globVar)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mGlobalVariableValues.add(varName, globVar);
    if (NS_FAILED(rv)) {
        delete globVar;
        return rv;
    }

    // Set up the state we have at the beginning of the transformation
    txVariableMap *oldVars = mLocalVariables;
    mLocalVariables = 0;
    txSingleNodeContext evalContext(mSourceNode, this);
    txIEvalContext* priorEC = setEvalContext(&evalContext);
    // Compute the variable value
    globVar->mFlags = GlobalVariableValue::evaluating;
    globVar->mValue = txXSLTProcessor::processVariable(varElem, this);
    setEvalContext(priorEC);
    mLocalVariables = oldVars;

    // evaluation is over, the gvv now owns the ExprResult
    globVar->mFlags = GlobalVariableValue::owned;
    aResult = globVar->mValue;
    return NS_OK;
}

/**
 * Determines if the given XML node allows Whitespace stripping
**/
MBool ProcessorState::isStripSpaceAllowed(Node* aNode)
{
    if (!aNode)
        return MB_FALSE;

    switch (aNode->getNodeType()) {
        case Node::ELEMENT_NODE:
        {
            // check Whitespace stipping handling list against given Node
            ImportFrame* frame;
            txListIterator frameIter(&mImportFrames);

            while ((frame = (ImportFrame*)frameIter.next())) {
                txListIterator iter(&frame->mWhiteNameTests);
                while (iter.hasNext()) {
                    txNameTestItem* iNameTest = (txNameTestItem*)iter.next();
                    if (iNameTest->matches(aNode, this)) {
                        if (iNameTest->stripsSpace() && 
                            !XMLUtils::getXMLSpacePreserve(aNode)) {
                            return MB_TRUE;
                        }
                        return MB_FALSE;
                    }
                }
            }
            break;
        }
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        {
            if (!XMLUtils::isWhitespace(aNode))
                return MB_FALSE;
            return isStripSpaceAllowed(aNode->getParentNode());
        }
        case Node::DOCUMENT_NODE:
        {
            return MB_TRUE;
        }
    }
    return MB_FALSE;
}

/**
 *  Notifies this Error observer of a new error using the given error level
**/
void ProcessorState::receiveError(const nsAString& errorMessage, nsresult aRes)
{
    txListIterator iter(&errorObservers);
    while (iter.hasNext()) {
        ErrorObserver* observer = (ErrorObserver*)iter.next();
        observer->receiveError(errorMessage, aRes);
    }
}

/**
 * Returns a call to the function that has the given name.
 * This method is used for XPath Extension Functions.
 * @return the FunctionCall for the function with the given name.
**/
#define CHECK_FN(_name) aName == txXSLTAtoms::_name

nsresult ProcessorState::resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                             Element* aElem,
                                             FunctionCall*& aFunction)
{
   aFunction = 0;

   if (aID != kNameSpaceID_None) {
       return NS_ERROR_XPATH_PARSE_FAILED;
   }
   if (CHECK_FN(document)) {
       aFunction = new DocumentFunctionCall(this, aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(key)) {
       aFunction = new txKeyFunctionCall(this, aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(formatNumber)) {
       aFunction = new txFormatNumberFunctionCall(this, aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(current)) {
       aFunction = new CurrentFunctionCall(this);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(unparsedEntityUri)) {
       return NS_ERROR_NOT_IMPLEMENTED;
   }
   if (CHECK_FN(generateId)) {
       aFunction = new GenerateIdFunctionCall();
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(systemProperty)) {
       aFunction = new SystemPropertyFunctionCall(aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(elementAvailable)) {
       aFunction = new ElementAvailableFunctionCall(aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }
   if (CHECK_FN(functionAvailable)) {
       aFunction = new FunctionAvailableFunctionCall(aElem);
       NS_ENSURE_TRUE(aFunction, NS_ERROR_OUT_OF_MEMORY);
       return NS_OK;
   }

   return NS_ERROR_XPATH_PARSE_FAILED;
} //-- resolveFunctionCall

  //-------------------/
 //- Private Methods -/
//-------------------/

ProcessorState::ImportFrame::ImportFrame(ImportFrame* aFirstNotImported)
    : mNamedTemplates(MB_FALSE),
      mMatchableTemplates(MB_TRUE),
      mNamedAttributeSets(MB_TRUE),
      mFirstNotImported(aFirstNotImported),
      mVariables(MB_FALSE)
{
}

ProcessorState::ImportFrame::~ImportFrame()
{
    // Delete all txNameTestItems
    txListIterator whiteIter(&mWhiteNameTests);
    while (whiteIter.hasNext())
        delete (txNameTestItem*)whiteIter.next();

    // Delete templates in mMatchableTemplates
    txExpandedNameMap::iterator iter(mMatchableTemplates);
    while (iter.next()) {
        txListIterator templIter((txList*)iter.value());
        MatchableTemplate* templ;
        while ((templ = (MatchableTemplate*)templIter.next())) {
            delete templ->mMatch;
            delete templ;
        }
    }
}

/*
 * txPSParseContext
 * txIParseContext used by ProcessorState internally
 */

nsresult txPSParseContext::resolveNamespacePrefix(nsIAtom* aPrefix,
                                                  PRInt32& aID)
{
#ifdef DEBUG
    if (!aPrefix || aPrefix == txXMLAtoms::_empty) {
        // default namespace is not forwarded to xpath
        NS_ASSERTION(0, "caller should handle default namespace ''");
        aID = kNameSpaceID_None;
        return NS_OK;
    }
#endif
    aID = mStyle->lookupNamespaceID(aPrefix);
    return (aID != kNameSpaceID_Unknown) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult txPSParseContext::resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                               FunctionCall*& aFunction)
{
    return mPS->resolveFunctionCall(aName, aID, mStyle, aFunction);
}

void txPSParseContext::receiveError(const nsAString& aMsg, nsresult aRes)
{
    mPS->receiveError(aMsg, aRes);
}

/*
 * GlobalVariableValue, Used avoid circular dependencies of variables
 */

ProcessorState::GlobalVariableValue::~GlobalVariableValue()
{
    NS_ASSERTION(mFlags != evaluating, "deleted while evaluating");
    if (mFlags == owned) {
        delete mValue;
    }
}
