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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#pragma once

typedef unsigned long MSG_ViewIndex;
typedef struct MSG_Pane MSG_Pane;

//========================================================================================
class CMailSelection 
// Represents a selection in a CMailFlexTable (which can be folders or messages).  Used
// for drag and drop, and all msglib command execution.
//========================================================================================
{
public:
					CMailSelection();
					CMailSelection(const CMailSelection& original)
					{
						*this = original; // the assignment op. doesn't need to destroy anything...
					}
	void /*sic*/	operator=(const CMailSelection& original);

	virtual const MSG_ViewIndex* GetSelectionList() const;

	void			SetSingleSelection(MSG_ViewIndex inSingleIndex)
					{	selectionSize = 1;
						singleIndex = inSingleIndex; 
						selectionList = &singleIndex;
					}
	void			Normalize();

public:	
	MSG_Pane*		xpPane;				// (threadpane or folder pane)
	MSG_ViewIndex	selectionSize;		// # of selected indices (1-based)	
	MSG_ViewIndex	singleIndex;		// Something for selectionList to point to, without
										// the need for an actual list to be present.  Useful for
										// the proxy drag case, e.g.
protected:
	friend class CMailFlexTable;
	MSG_ViewIndex*	selectionList;		// List of selected indices (zero-based),
										// a reference to the mSelectionList in the table,
										// so none of this data needs to be deleted by this
										// object.
}; // class CMailSelection

//========================================================================================
class CPersistentMailSelection
// "Owns" its data.  Clones data when duplicated, destroys data when deleted.
// The list of selected items can either be a list of indices (volatile) or a list of
// persistent keys (persistent).  This state is indicated by the mDataIsVolatile member.
// Constructors and assignment operators always convert any non-trivial data to persistent
// form.
// GetSelectionList() still returns a list of indices (ie, the volatile form of the data),
// so it converts to volatile form before returning the result.
// This is an abstract class, because the form of the persistent key is particular to the
// view.  See also the derived classes, CPersistentFolderSelection, and
// CPersistentMessageSelection.  These classes override the MakePersistent() and
// MakeVolatile() methods.
//========================================================================================
:	public CMailSelection
{
	typedef CMailSelection Inherited;

public:
					// No copy constructor in the base class.!
					// Since MakeAllPersistent() needs virtual functions, and we
					// need the inherited behavior, we can't implement a copy constructor (indeed,
					// it will crash if we do.  Therefore we provide only a void constructor,
					// forcing clients to use the assignment operator, which is safe.
					CPersistentMailSelection()
						:	mDataIsVolatile (true)
					{
					}
	virtual			~CPersistentMailSelection();

	void /*sic*/	operator=(const CMailSelection& original)
					{
						DestroyData(); // like all good little assignment operators
						CMailSelection::operator=(original);
						mDataIsVolatile = true;
						CloneData();
						MakeAllPersistent();
					}
	virtual const MSG_ViewIndex* GetSelectionList() const;

protected:
	// We need versions of these for folders (MSG_FolderInfo*) and messages (MessageKey)
	// See CPersistentFolderSelection and CPersistentMessageSelection.
	virtual	void	MakePersistent(MSG_ViewIndex& ioKey) const = 0;
	virtual void	MakeVolatile(MSG_ViewIndex& ioKey) const = 0;

	void			MakeAllVolatile()
					{
						if (mDataIsVolatile)
							return;
						MSG_ViewIndex* cur = selectionList;
						for (int i = 0; i < selectionSize; i++,cur++)
							MakeVolatile(*cur);
						mDataIsVolatile = true;
					}
	void			MakeAllPersistent()
					{
						if (!mDataIsVolatile)
							return;
						MSG_ViewIndex* cur = selectionList;
						for (int i = 0; i < selectionSize; i++,cur++)
							MakePersistent(*cur);
						mDataIsVolatile = false;
					}
	void			CloneData();

	void			DestroyData();

	// data
	Boolean			mDataIsVolatile;
}; // class CPersistentMailSelection

//========================================================================================
class CPersistentFolderSelection
//========================================================================================
:	public CPersistentMailSelection
{
	public:
									CPersistentFolderSelection(const CMailSelection& orig)
										: CPersistentMailSelection()
									{
										// Void constructor of base class has now been
										// called. Call the assignment operator to copy
										// and convert.
										CPersistentMailSelection::operator=(orig);
									}
		virtual	void				MakePersistent(MSG_ViewIndex& ioKey) const;
		virtual	void				MakeVolatile(MSG_ViewIndex& ioKey) const;
}; // class CPersistentFolderSelection

//========================================================================================
class CPersistentMessageSelection
//========================================================================================
:	public CPersistentMailSelection
{
	public:
								CPersistentMessageSelection(const CMailSelection& original)
									: CPersistentMailSelection()
								{
									// Void constructor of base class has now been called.
									// Call the assignment operator to copy and convert.
									CPersistentMailSelection::operator=(original);
								}
		virtual	void			MakePersistent(MSG_ViewIndex& ioKey) const;
		virtual	void			MakeVolatile(MSG_ViewIndex& ioKey) const;
}; // class CPersistentMessageSelection
