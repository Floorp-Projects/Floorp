/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
	nsMsgSearchBoolExpression(nsIMsgSearchTerm * newTerm,
                              PRBool EvaluationValue = PR_TRUE,
                              char * encodingStr = NULL);         
	nsMsgSearchBoolExpression(nsIMsgSearchTerm * newTerm, char * encodingStr);

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
	nsMsgSearchBoolExpression * AddSearchTerm (nsIMsgSearchTerm * newTerm,
                                               PRBool EvaluationValue = PR_TRUE);
	nsMsgSearchBoolExpression * AddSearchTerm (nsIMsgSearchTerm * newTerm,
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
	void GenerateEncodeStr(nsCString * buffer); 
protected:
	// if we are a leaf node, all we have is a search term
    // and a Evaluation value for that search term
    
    nsIMsgSearchTerm * m_term;
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
	nsMsgSearchBoolExpression * leftToRightAddTerm(nsIMsgSearchTerm * newTerm,
                                                   PRBool EvaluationValue,
                                                   char * encodingStr); 
};

#endif
