/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef __nsDirectoryDataSource_h
#define __nsDirectoryDataSource_h
 

#include "nsAbRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIAbListener.h"
#include "nsIAbDirectory.h"
#include "nsDirPrefs.h"
#include "nsIAbListener.h"
#include "nsISupportsArray.h"

/**
 * The addressbook data source.
 */
class nsAbDirectoryDataSource : public nsAbRDFDataSource,
							    public nsIAbListener
{
private:
	PRBool	mInitialized;

	// The cached service managers
	nsIRDFService* mRDFService;

public:
  
	NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIABLISTENER
	nsAbDirectoryDataSource(void);
	virtual ~nsAbDirectoryDataSource (void);
	virtual nsresult Init();

	// nsIRDFDataSource methods
	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetTarget(nsIRDFResource* source,
					   nsIRDFResource* property,
					   PRBool tv,
					   nsIRDFNode** target);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsISimpleEnumerator** targets);

	NS_IMETHOD Assert(nsIRDFResource* source,
					nsIRDFResource* property, 
					nsIRDFNode* target,
					PRBool tv);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
						  nsIRDFResource* property,
						  nsIRDFNode* target,
						  PRBool tv,
						  PRBool* hasAssertion);

	NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result);

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
						  nsISimpleEnumerator** labels); 

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments);

protected:

	nsresult createDirectoryNode(nsIAbDirectory* directory, nsIRDFResource* property,
                                 nsIRDFNode** target);
	nsresult createDirectoryNameNode(nsIAbDirectory *directory,
                                     nsIRDFNode **target);
	nsresult createDirectoryUriNode(nsIAbDirectory *directory,
                                     nsIRDFNode **target);
	nsresult createDirectoryChildNode(nsIAbDirectory *directory,
                                      nsIRDFNode **target);
	nsresult createDirectoryIsMailListNode(nsIAbDirectory *directory,
                                      nsIRDFNode **target);
	static nsresult getDirectoryArcLabelsOut(nsIAbDirectory *directory,
										   nsISupportsArray **arcs);

	nsresult DoDeleteFromDirectory(nsISupportsArray *parentDirs,
							  nsISupportsArray *delDirs);
	nsresult DoDeleteCardsFromDirectory(nsIAbDirectory *directory,
							  nsISupportsArray *delDirs);

	nsresult DoDirectoryAssert(nsIAbDirectory *directory, 
					nsIRDFResource *property, nsIRDFNode *target);
	nsresult DoDirectoryHasAssertion(nsIAbDirectory *directory, 
							 nsIRDFResource *property, nsIRDFNode *target,
							 PRBool tv, PRBool *hasAssertion);
	nsresult DoNewDirectory(nsIAbDirectory *directory, nsISupportsArray *arguments);

	nsresult CreateLiterals(nsIRDFService *rdf);
	nsresult GetTargetHasAssertion(nsIRDFDataSource *dataSource, nsIRDFResource* dirResource,
							   nsIRDFResource *property,PRBool tv, nsIRDFNode *target,PRBool* hasAssertion);

	static nsIRDFResource* kNC_Child;
	static nsIRDFResource* kNC_DirName;
	static nsIRDFResource* kNC_CardChild;
	static nsIRDFResource* kNC_DirUri;
	static nsIRDFResource* kNC_IsMailList;

	// commands
	static nsIRDFResource* kNC_Delete;
	static nsIRDFResource* kNC_DeleteCards;
	static nsIRDFResource* kNC_NewDirectory;

	//Cached literals
	nsCOMPtr<nsIRDFNode> kTrueLiteral;
	nsCOMPtr<nsIRDFNode> kFalseLiteral;
};

nsresult NS_NewAbDirectoryDataSource(const nsIID& iid, void **result);


#endif
