/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMsgSearchCore.h"

#ifndef __nsMsgSearchBoolExpression_h
#define __nsMsgSearchBoolExpression_h

//-----------------------------------------------------------------------------
// nsMsgSearchBoolExpression is a class added to provide AND/OR terms in search queries. 
//	A nsMsgSearchBoolExpression contains either a search term or two nsMsgSearchBoolExpressions and
//    a boolean operator.
// I (mscott) am placing it here for now....
//-----------------------------------------------------------------------------

/* CBoolExpresion --> encapsulates one or more search terms by internally
   representing the search terms and their boolean operators as a binary
   expression tree. Each node in the tree consists of either
     (1) a boolean operator and two nsMsgSearchBoolExpressions or 
	 (2) if the node is a leaf node then it contains a search term.
   With each search term that is part of the expression we also keep
   track of either an evaluation value (XP_BOOL) or a character string.
   Evaluation values are used for offline searching. The character 
   string is used to store the IMAP/NNTP encoding of the search term. This
   makes evaluating the expression (for offline) or generating a search
   encoding (for online) easier.

   For IMAP/NNTP: nsMsgSearchBoolExpression has/assumes knowledge about how
   AND and OR search terms are combined according to IMAP4 and NNTP protocol.
   That is the only piece of IMAP/NNTP knowledge it is aware of. 

   Order of Evaluation: Okay, the way in which the boolean expression tree
   is put together directly effects the order of evaluation. We currently
   support left to right evaluation. 
   Supporting other order of evaluations involves adding new internal add
   term methods. 
 */

class nsMsgSearchBoolExpression 
{
public:

	// create a leaf node expression
	nsMsgSearchBoolExpression(nsMsgSearchTerm * newTerm,
                              PRBool EvaluationValue = PR_TRUE,
                              char * encodingStr = NULL);         
	nsMsgSearchBoolExpression(nsMsgSearchTerm * newTerm, char * encodingStr);

	// create a non-leaf node expression containing 2 expressions
    // and a boolean operator
	nsMsgSearchBoolExpression(nsMsgSearchBoolExpression *,
                              nsMsgSearchBoolExpression *,
                              nsMsgSearchBooleanOperator boolOp); 
	
	nsMsgSearchBoolExpression();
	~nsMsgSearchBoolExpression();  // recursively destroys all sub
                                   // expressions as well

	// accesors
    
    // Offline
	nsMsgSearchBoolExpression * AddSearchTerm (nsMsgSearchTerm * newTerm,
                                               PRBool EvaluationValue = PR_TRUE);
	nsMsgSearchBoolExpression * AddSearchTerm (nsMsgSearchTerm * newTerm,
                                               char * encodingStr); // IMAP/NNTP

    // parses the expression tree and all
    // expressions underneath this node using
    // each EvaluationValue at each leaf to
    // determine if the end result is PR_TRUE or PR_FALSE.
	PRBool OfflineEvaluate();
    
    // assuming the expression is for online
    // searches, determine the length of the
    // resulting IMAP/NNTP encoding string
	PRInt32 CalcEncodeStrSize();
    
    // fills pre-allocated
    // memory in buffer with 
    // the IMAP/NNTP encoding for the expression
    // returns # bytes added to the buffer
	PRInt32 GenerateEncodeStr(nsCString * buffer); 
protected:
	// if we are a leaf node, all we have is a search term
    // and a Evaluation value for that search term
    
	nsMsgSearchTerm * m_term;
	PRBool m_evalValue;
    
    // store IMAP/NNTP encoding for the search term if applicable
	nsCString m_encodingStr;     

	// if we are not a leaf node, then we have two other expressions
    // and a boolean operator
	nsMsgSearchBoolExpression * m_leftChild;
	nsMsgSearchBoolExpression * m_rightChild;
	nsMsgSearchBooleanOperator m_boolOp;


	// internal methods

	// the idea is to separate the public interface for adding terms to
    // the expression tree from the order of evaluation which influences
    // how we internally construct the tree. Right now, we are supporting
    // left to right evaluation so the tree is constructed to represent
    // that by calling leftToRightAddTerm. If future forms of evaluation
    // need to be supported, add new methods here for proper tree construction.
	nsMsgSearchBoolExpression * leftToRightAddTerm(nsMsgSearchTerm * newTerm,
                                                   PRBool EvaluationValue,
                                                   char * encodingStr); 
};

#endif
