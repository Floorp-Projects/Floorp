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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _ABINFO_H
#define _ABINFO_H

const int kNumUnused = 20;
const int kAddrInfoVersion = 1;

class ABNeoABInfo : public CNeoPersistNative
{
public:
						/** Instance Methods **/
						ABNeoABInfo();
	static CNeoPersist *New(void);
	void				SetSortByFirstName(Bool sortby = TRUE) { m_sortByFirstName = sortby; setDirty();};
	XP_Bool				GetSortByFirstName() {return m_sortByFirstName;};

	virtual NeoID		getClassID(void) const {return kNeoABInfoID;};
	virtual long		getFileLength(const CNeoFormat *aFormat) const;

						/** I/O Methods **/
	virtual void		readObject(CNeoStream *aStream, const NeoTag aTag);
	virtual void		writeObject(CNeoStream *aStream, const NeoTag aTag);

#if defined(qNeoTrace_mccusker) || defined(qNeoXml_mccusker)
	virtual const char*	classDebugName() const;
	//virtual char*		asXmlString(char* buf256) const; // buf256 must be >= 256 bytes
	//virtual void		traceObject() const;
#endif

							/** Schema-Evolution Methods **/
	virtual NeoMark		convert(CNeoFormat *aOldFormat, CNeoFormat *aNewFormat);

public:

#ifdef qNeoDebug
						/** Debugging Methods **/
	virtual const void 	*verify(const void *aValue) const;
#endif

protected:
	XP_Bool				m_sortByFirstName;

	// db object on disk contains kNumUnused more uint32's for future expansion
};


#endif

