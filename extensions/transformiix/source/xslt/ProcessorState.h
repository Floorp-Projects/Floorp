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
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */



#ifndef TRANSFRMX_PROCESSORSTATE_H
#define TRANSFRMX_PROCESSORSTATE_H

#include "NodeSet.h"
#include "ErrorObserver.h"
#include "txPatternParser.h"
#include "Expr.h"
#include "txOutputFormat.h"
#include "Map.h"
#include "txIXPathContext.h"
#include "txExpandedNameMap.h"
#include "txXMLEventHandler.h"
#include "XSLTFunctions.h"
#include "txError.h"
#include "nsVoidArray.h"
#include "nsDoubleHashtable.h"

class txVariableMap;
class txXSLKey;

class txLoadedDocumentEntry : public PLDHashStringEntry {
public:
    txLoadedDocumentEntry(const void* aKey) : PLDHashStringEntry(aKey)
    {
    }
    ~txLoadedDocumentEntry()
    {
        delete mDocument;
    }
    Document* mDocument;
};

DECL_DHASH_WRAPPER(txLoadedDocumentsBase, txLoadedDocumentEntry, nsAString&)

class txLoadedDocumentsHash : public txLoadedDocumentsBase
{
public:
    txLoadedDocumentsHash(Document* aSourceDocument, Document* aStyleDocument);
    ~txLoadedDocumentsHash();
    void Add(Document* aDocument);
    Document* Get(const nsAString& aURI);

private:
    friend class ProcessorState;
    Document* mSourceDocument;
    Document* mStyleDocument;
};

/**
 * Class used for keeping the current state of the XSL Processor
 */
class ProcessorState : public txIMatchContext {

public:
    /**
     * Creates a new ProcessorState for the given XSL document
     * And result Document
     */
    ProcessorState(Document* aSourceDocument,
                   Document* aXslDocument);

    /**
     * Destroys this ProcessorState
     */
    ~ProcessorState();

    /**
     * Contain information that is import precedence dependant.
     */
    class ImportFrame {
    public:
        ImportFrame(ImportFrame* aFirstNotImported);
        ~ImportFrame();
    
        // Map of named templates
        txExpandedNameMap mNamedTemplates;

        // Map of template modes, each item in the map is a list
        // of templates
        txExpandedNameMap mMatchableTemplates;

        // List of whitespace preserving and stripping nametests
        txList mWhiteNameTests;

        // Map of named attribute sets
        txExpandedNameMap mNamedAttributeSets;

        // Output format, as specified by the xsl:output elements
        txOutputFormat mOutputFormat;

        // ImportFrame which is the first one *not* imported by this frame
        ImportFrame* mFirstNotImported;

        // Map of top-level variables/parameters
        txExpandedNameMap mVariables;

        // The following stuff is missing here:

        // Namespace aliases (xsl:namespace-alias)
    };
    // To be able to do some cleaning up in destructor
    friend class ImportFrame;

    /**
     * Adds the given attribute set to the list of available named attribute
     * sets
     * @param aAttributeSet the Element to add as a named attribute set
     * @param aImportFrame  ImportFrame to add the attributeset to
     */
    void addAttributeSet(Element* aAttributeSet, ImportFrame* aImportFrame);

    /**
     * Registers the given ErrorObserver with this ProcessorState
     */
    void addErrorObserver(ErrorObserver& errorObserver);

    /**
     * Adds the given template to the list of templates to process
     * @param aXslTemplate  The Element to add as a template
     * @param aImportFrame  ImportFrame to add the template to
     */
    void addTemplate(Element* aXslTemplate, ImportFrame* aImportFrame);

    /**
     * Adds the given LRE Stylesheet to the list of templates to process
     * @param aStylesheet  The Stylesheet to add as a template
     * @param importFrame  ImportFrame to add the template to
     */
    void addLREStylesheet(Document* aStylesheet, ImportFrame* aImportFrame);

    /**
     * Returns the AttributeSet associated with the given name
     * or null if no AttributeSet is found
     */
    NodeSet* getAttributeSet(const txExpandedName& aName);

    /**
     * Returns the template associated with the given name, or
     * null if not template is found
     */
    Element* getNamedTemplate(const txExpandedName& aName);

    /**
     * Returns the OutputFormat which contains information on how
     * to serialize the output.
     */
    txOutputFormat* getOutputFormat();

    /**
     * Add a global variable
     */
    nsresult addGlobalVariable(const txExpandedName& aVarName,
                               Element* aVarElem,
                               ImportFrame* aImportFrame,
                               ExprResult* aDefaultValue);

    /**
     * Returns map on top of the stack of local variable-bindings
     */
    txVariableMap* getLocalVariables();
    
    /**
     * Sets top map of the local variable-bindings stack
     */
    void setLocalVariables(txVariableMap* aMap);

    /**
     * Enums for the getExpr and getPattern functions
     */
    enum ExprAttr {
        SelectAttr = 0,
        TestAttr,
        ValueAttr
    };

    enum PatternAttr {
        CountAttr = 0,
        FromAttr
    };

    Expr* getExpr(Element* aElem, ExprAttr aAttr);
    txPattern* getPattern(Element* aElem, PatternAttr aAttr);

    /**
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
    Node* retrieveDocument(const nsAString& uri, const nsAString& baseUri);

    /**
     * Return stack of urls of currently entered stylesheets
     */
    nsStringArray& getEnteredStylesheets()
    {
        return mEnteredStylesheets;
    }

    /**
     * Return list of import containers
     */
    List* getImportFrames();

    /**
     * Find template in specified mode matching the supplied node
     * @param aNode        node to find matching template for
     * @param aMode        mode of the template
     * @param aImportFrame out-param, is set to the ImportFrame containing
     *                     the found template
     * @return             root-node of found template, null if none is found
     */
    Node* findTemplate(Node* aNode,
                       const txExpandedName& aMode,
                       ImportFrame** aImportFrame)
    {
        return findTemplate(aNode, aMode, 0, aImportFrame);
    }

    /**
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
    Node* findTemplate(Node* aNode,
                       const txExpandedName& aMode,
                       ImportFrame* aImportedBy,
                       ImportFrame** aImportFrame);

    /**
     * Struct holding information about a current template rule
     */
    struct TemplateRule {
        ImportFrame* mFrame;
        const txExpandedName* mMode;
        txVariableMap* mParams;
    };

    /**
     * Gets current template rule
     */
    TemplateRule* getCurrentTemplateRule();

    /**
     * Sets current template rule
     */
    void setCurrentTemplateRule(TemplateRule* aTemplateRule);

    /**
     * Adds the set of names to the Whitespace preserving element set
     */
    void preserveSpace(nsAString& names);

    void processAttrValueTemplate(const nsAFlatString& aAttValue,
                                  Element* aContext,
                                  nsAString& aResult);

    /**
     * Adds the set of names to the Whitespace stripping handling list.
     * xsl:strip-space calls this with MB_TRUE, xsl:preserve-space 
     * with MB_FALSE
     * aElement is used to resolve QNames
     */
    void shouldStripSpace(const nsAString& aNames, Element* aElement,
                          MBool aShouldStrip,
                          ImportFrame* aImportFrame);

    /**
     * Adds the supplied xsl:key to the set of keys
     */
    MBool addKey(Element* aKeyElem);

    /**
     * Returns the key with the supplied name
     * returns NULL if no such key exists
     */
    txXSLKey* getKey(txExpandedName& keyName);

    /**
     * Adds a decimal format. Returns false if the format already exists
     * but dosn't contain the exact same parametervalues
     */
    MBool addDecimalFormat(Element* element);

    /**
     * Returns a decimal format or NULL if no such format exists.
     */
    txDecimalFormat* getDecimalFormat(const txExpandedName& name);

    /**
     * Returns a pointer to a document that can be used to create RTFs
     */
    Document* getRTFDocument();

    /**
     * Sets a new document to be used for creating RTFs
     */
    void setRTFDocument(Document* aDoc);

    /**
     * Returns the stylesheet document
     */
    Document* getStylesheetDocument();

    /**
     * Virtual methods from txIEvalContext
     */

    TX_DECL_MATCH_CONTEXT;

    /**
     * Set the current txIEvalContext and get the prior one
     */
    txIEvalContext* setEvalContext(txIEvalContext* aEContext)
    {
        txIEvalContext* tmp = mEvalContext;
        mEvalContext = aEContext;
        return tmp;
    }

    /**
     * Get the current txIEvalContext
     */
    txIEvalContext* getEvalContext()
    {
        return mEvalContext;
    }


    /**
     * More other functions
     */
    void receiveError(const nsAString& errorMessage)
    {
        receiveError(errorMessage, NS_ERROR_FAILURE);
    }

    /**
     * Returns a FunctionCall which has the given name.
     * @return the FunctionCall for the function with the given name.
     */
    nsresult resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                 Element* aElem, FunctionCall*& aFunction);

#ifdef TX_EXE
    txIOutputXMLEventHandler* mOutputHandler;
#else
    nsCOMPtr<txIOutputXMLEventHandler> mOutputHandler;
#endif
    txXMLEventHandler* mResultHandler;
    
    txIOutputHandlerFactory* mOutputHandlerFactory;

private:

    class MatchableTemplate {
    public:
        MatchableTemplate(Node* aTemplate, txPattern* aPattern,
                          double aPriority)
            : mTemplate(aTemplate), mMatch(aPattern), mPriority(aPriority)
        {
        }
        Node* mTemplate;
        txPattern* mMatch;
        double mPriority;
    };
    
    class GlobalVariableValue : public TxObject {
    public:
        GlobalVariableValue(ExprResult* aValue = 0)
            : mValue(aValue), mFlags(nonOwned)
        {
        }
        virtual ~GlobalVariableValue();
      
        ExprResult* mValue;
        char mFlags;
        enum _flags 
        {
            nonOwned = 0,
            evaluating,
            owned
        };
    };

    /**
     * The list of ErrorObservers registered with this ProcessorState
     */
    List errorObservers;

    /**
     * Stack of URIs for currently entered stylesheets
     */
    nsStringArray mEnteredStylesheets;

    /**
     * List of import containers. Sorted by ascending import precedence
     */
    txList mImportFrames;

    /**
     * The output format used when serializing the result
     */
    txOutputFormat mOutputFormat;

    /**
     * The set of loaded documents. This includes both document() loaded
     * documents and xsl:include/xsl:import'ed documents.
     */
    txLoadedDocumentsHash mLoadedDocuments;
    
    /**
     * The set of all available keys
     */
    txExpandedNameMap mXslKeys;

    /**
     * The set of all avalible decimalformats
     */
    txExpandedNameMap mDecimalFormats;
    
    /**
     * Default decimal-format
     */
    txDecimalFormat mDefaultDecimalFormat;

    /**
     * List of hashes with parsed expression. Every listitem holds the
     * expressions for an attribute name
     */
    Map            mExprHashes[3];

    /**
     * List of hashes with parsed patterns. Every listitem holds the
     * patterns for an attribute name
     */
    Map            mPatternHashes[2];

    /**
     * Current txIEvalContext*
     */
    txIEvalContext* mEvalContext;

    /**
     * Current template rule
     */
    TemplateRule*  mCurrentTemplateRule;

    /**
     * Top of stack of local variable-bindings
     */
    txVariableMap* mLocalVariables;
    
    /**
     * Map of values of global variables
     */
    txExpandedNameMap mGlobalVariableValues;

    /**
     * Document used to create RTFs
     */
    Document* mRTFDocument;
};

/**
 * txNameTestItem holds both an ElementExpr and a bool for use in
 * whitespace stripping.
 */
class txNameTestItem {
public:
    txNameTestItem(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID,
                   MBool stripSpace)
        : mNameTest(aPrefix, aLocalName, aNSID, Node::ELEMENT_NODE),
          mStrips(stripSpace)
    {
    }

    MBool matches(Node* aNode, txIMatchContext* aContext) {
        return mNameTest.matches(aNode, aContext);
    }

    MBool stripsSpace() {
        return mStrips;
    }

    double getDefaultPriority() {
        return mNameTest.getDefaultPriority();
    }

protected:
    txNameTest mNameTest;
    MBool mStrips;
};

/**
 * txPSParseContext
 * a txIParseContext forwarding all but resolveNamespacePrefix
 * to a ProcessorState
 */
class txPSParseContext : public txIParseContext {
public:
    txPSParseContext(ProcessorState* aPS, Element* aStyleNode)
        : mPS(aPS), mStyle(aStyleNode)
    {
    }

    ~txPSParseContext()
    {
    }

    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, PRInt32& aID);
    nsresult resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                 FunctionCall*& aFunction);
    void receiveError(const nsAString& aMsg, nsresult aRes);

protected:
    ProcessorState* mPS;
    Element* mStyle;
};

#endif

