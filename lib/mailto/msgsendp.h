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
#ifndef MSG_SENDP_H
#define MSG_SENDP_H


#include "msgzap.h"
#include "mimeenc.h"

class MSG_SendMimeDeliveryState;

typedef int (*MSG_SendPartWriteFunc)(const char* line, int32 size,
									 XP_Bool isheader, void* closure);


class MSG_SendPart : public MSG_ZapIt {
public:
    MSG_SendPart(MSG_SendMimeDeliveryState* state, int16 part_csid = 0);
    virtual ~MSG_SendPart();	// Note that the destructor also destroys
								// any children that were added.

    virtual int Write();

    virtual int SetFile(const char* filename, XP_FileType filetype);
    const char* GetFilename() {return m_filename;}
	XP_FileType GetFiletype() {return m_filetype;}

    virtual int SetBuffer(const char* buffer);
    const char* GetBuffer() {return m_buffer;}

    virtual int SetType(const char* type);
    const char* GetType() {return m_type;}
    
    int16 GetCSID() { return m_csid; }

    virtual int SetOtherHeaders(const char* other);
    const char* SetOtherHeaders() {return m_other;}
	virtual int AppendOtherHeaders(const char* moreother);

	virtual int SetMimeDeliveryState(MSG_SendMimeDeliveryState* state);

	// Note that the MSG_SendPart class will take over ownership of the
	// MimeEncoderData* object, deleting it when it chooses.  (This is
	// necessary because deleting these objects is the only current way to
	// flush out the data in them.)
	int SetEncoderData(MimeEncoderData* data);
	MimeEncoderData *GetEncoderData() {return m_encoder_data;}

	int SetStripSensitiveHeaders(XP_Bool value) {
		m_strip_sensitive_headers = value;
		return 0;
	}
	XP_Bool GetStripSensitiveHeaders() {return m_strip_sensitive_headers;}

    virtual int AddChild(MSG_SendPart* child);

	int32 GetNumChildren() {return m_numchildren;}
	MSG_SendPart* GetChild(int32 which);
	MSG_SendPart* DetachChild(int32 which);

	virtual int SetMainPart(XP_Bool value);
	XP_Bool IsMainPart() {return m_mainpart;}


protected:
	int CopyString(char** dest, const char* src);
	int PushBody(char* buffer, int32 length);

	MSG_SendMimeDeliveryState* m_state;
	MSG_SendPart* m_parent;
    char* m_filename;
	XP_FileType m_filetype;
	char* m_buffer;
    char* m_type;
    char* m_other;
	int16 m_csid;	// charset ID associated with this part
	XP_Bool m_strip_sensitive_headers;
	MimeEncoderData *m_encoder_data;  /* Opaque state for base64/qp encoder. */

	MSG_SendPart** m_children;
	int32 m_numchildren;

	// Data used while actually writing.
    XP_Bool m_firstBlock;
    XP_Bool m_needIntlConversion;
    CCCDataObject m_intlDocToMailConverter;

	XP_Bool m_mainpart;

	XP_Bool m_just_hit_CR;

	static int32 M_counter;
};



#endif /* MSG_SENDP_H */
